#include "iPiMocapLiveLinkSource.h"

#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkAnimationTypes.h"
#include "Roles/LiveLinkAnimationRole.h"

#include "Common/UdpSocketBuilder.h"
#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#define LOCTEXT_NAMESPACE "FiPiMocapLiveLinkSource"

#define RECV_BUFFER_SIZE 1024 * 1024

FiPiMocapLiveLinkSource::FiPiMocapLiveLinkSource(FIPv4Endpoint InEndpoint)
	: Socket{ nullptr }
	, Stopping{ false }
	, Thread{ nullptr }
	, WaitTime{ FTimespan::FromMilliseconds(10) }
	, SignatureLength{ (int32)strlen(SIGNATURE) }
	, MinPacketSize{ SignatureLength + (int32)sizeof(int32)}
{
	// defaults
	DeviceEndpoint = InEndpoint;

	SourceStatus = LOCTEXT("SourceStatus_DeviceNotFound", "Device Not Found");
	SourceType = LOCTEXT("iPiMocapLiveLinkSource", "iPi Mocap Live Link");
	SourceMachineName = FText::Format(LOCTEXT("iPiMocapLiveLinkSourceMachineName", "{0}:{1}"),
		FText::FromString(DeviceEndpoint.Address.ToString()), FText::AsNumber(DeviceEndpoint.Port, &FNumberFormattingOptions::DefaultNoGrouping()));

	SubjectName = "Character";

	Socket = FUdpSocketBuilder(TEXT("IPIMOCAPSOCKET"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToAddress(DeviceEndpoint.Address)
		.BoundToPort(DeviceEndpoint.Port)
		.WithReceiveBufferSize(RECV_BUFFER_SIZE);

	RecvBuffer.SetNumUninitialized(RECV_BUFFER_SIZE);

	if ((Socket != nullptr) && (Socket->GetSocketType() == SOCKTYPE_Datagram))
	{
		RecvBuffer.SetNumUninitialized(RECV_BUFFER_SIZE);
		SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		Start();
		SourceStatus = LOCTEXT("SourceStatus_Receiving", "Receiving");
	}
}

FiPiMocapLiveLinkSource::~FiPiMocapLiveLinkSource()
{
	Stop();

	if (Socket != nullptr)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
	}
}

void FiPiMocapLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;
}


bool FiPiMocapLiveLinkSource::IsSourceStillValid() const
{
	bool bIsSourceValid = !Stopping && Thread != nullptr && Socket != nullptr;
	return bIsSourceValid;
}


bool FiPiMocapLiveLinkSource::RequestSourceShutdown()
{
	Stop();

	return true;
}
// FRunnable interface

void FiPiMocapLiveLinkSource::Start()
{
    Stopping = false;

    if (Thread == nullptr)
    {
        ThreadName = "iPi Mocap UDP Receiver ";
        ThreadName.AppendInt(FAsyncThreadIndex::GetNext());
        Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
    }
}

void FiPiMocapLiveLinkSource::Stop()
{
    Stopping = true;

    if (Thread != nullptr)
    {
        Thread->WaitForCompletion();
        Thread = nullptr;
    }
}

uint32 FiPiMocapLiveLinkSource::Run()
{
	TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();
	
	while (!Stopping)
	{
		if (Socket->Wait(ESocketWaitConditions::WaitForRead, WaitTime))
		{
			uint32 Size;

			while (Socket->HasPendingData(Size))
			{
				int32 bytesRead = 0;

				if (Socket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), bytesRead, *Sender))
				{
					if (bytesRead >= MinPacketSize)
					{
						if (!memcmp(RecvBuffer.GetData(), SIGNATURE, SignatureLength))
						{
							iPiBinaryReader reader((unsigned char*)RecvBuffer.GetData() + SignatureLength, bytesRead - SignatureLength);
							
							int32 packet_kind;
							if (reader.Read(packet_kind))
							{
								if (packet_kind == PACKET_KIND_HIERARCHY)
								{
									ReadAndSetSkeletonFrameData(reader);
								}
								else if (packet_kind == PACKET_KIND_POSE && JointsToStream.Num() > 0)
								{
									ReadAndSetAnimationFrameData(reader);
								}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}

void FiPiMocapLiveLinkSource::ReadAndSetSkeletonFrameData(iPiBinaryReader& reader)
{
	FScopeLock hLock(&hierarchyDataLock);

	int32 hipsJointIdx;
	if (!reader.Read(hipsJointIdx))
	{
		return;
	}

	int32 jointCount;
	if (!reader.Read(jointCount))
	{
		return;
	}

	int32 jointsRead = 0;
	TArray<FStreamHierarchy> newJoints;

	for (auto jointIndex = 0; jointIndex < jointCount; jointIndex++)
	{
		FString jointNameStr;
		if (!reader.ReadString(jointNameStr))
		{
			break;
		}

		const FName jointName = *jointNameStr;

		int32 parentIndex;	
		if (!reader.Read(parentIndex))
		{
			break;
		}

		FVector defaultOffset = FVector::ZeroVector;
		if (reader.ReadVector(defaultOffset))
		{
			defaultOffset.Y = -defaultOffset.Y; // change Y direction from right-handed to left-handed
		}
		else
		{
			break;
		}

		newJoints.Add(FStreamHierarchy(jointName, parentIndex, defaultOffset, hipsJointIdx == jointIndex));
		jointsRead++;
	}

	if (jointsRead == jointCount
		&& JointsToStream != newJoints)
	{
		JointsToStream = MoveTemp(newJoints);

		FLiveLinkStaticDataStruct StaticData(FLiveLinkSkeletonStaticData::StaticStruct());
		FLiveLinkSkeletonStaticData& SkeletonData = *StaticData.Cast<FLiveLinkSkeletonStaticData>();
		for (auto& jointDesc : JointsToStream)
		{
			SkeletonData.BoneNames.Add(jointDesc.JointName);
			SkeletonData.BoneParents.Add(jointDesc.ParentIndex);
		}

		Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkAnimationRole::StaticClass(), MoveTemp(StaticData));
	}
}

void FiPiMocapLiveLinkSource::ReadAndSetAnimationFrameData(iPiBinaryReader& reader)
{
	FVector rootLocation = FVector::ZeroVector;

	if (reader.ReadVector(rootLocation))
	{
		rootLocation.Y = -rootLocation.Y; // change Y direction from right-handed to left-handed
	}
	else
	{
		return;
	}

	int32 rotationCount;
	if (!reader.Read(rotationCount))
	{
		return;
	}

	FScopeLock hLock(&hierarchyDataLock);
	if (rotationCount == JointsToStream.Num())
	{
		FLiveLinkFrameDataStruct FrameData(FLiveLinkAnimationFrameData::StaticStruct());
		FLiveLinkAnimationFrameData& AnimationData = *FrameData.Cast<FLiveLinkAnimationFrameData>();

		for (int i = 0; i < rotationCount; i++)
		{
			auto& jointDesc = JointsToStream[i];
			FVector tLocation = (jointDesc.ApplyTranslation ? rootLocation : jointDesc.DefaultOffset) * 100.0; // meters to cm

			FQuat tQuat = FQuat::Identity;
			if (reader.ReadQuaternion(tQuat))
			{
				tQuat.X = -tQuat.X; // change Y direction from right-handed to left-handed
				tQuat.Z = -tQuat.Z; // change Y direction from right-handed to left-handed
			}
			else
			{
				return;
			}

			AnimationData.Transforms.Add(FTransform{ tQuat, tLocation, FVector::OneVector });
		}

		Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(FrameData));
	}
}

#undef LOCTEXT_NAMESPACE
