#pragma once

#include "ILiveLinkSource.h"
#include "HAL/Runnable.h"
#include "IMessageContext.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/ScopeLock.h"

#include "iPiBinaryReader.h"

class FRunnableThread;
class FSocket;
class ILiveLinkClient;
class ISocketSubsystem;

struct FStreamHierarchy
{
	FName JointName;
	int32 ParentIndex;
	FVector DefaultOffset;
	bool ApplyTranslation;

	FStreamHierarchy() {}

	FStreamHierarchy(const FStreamHierarchy& Other)
		: JointName(Other.JointName)
		, ParentIndex(Other.ParentIndex)
		, DefaultOffset(Other.DefaultOffset)
		, ApplyTranslation(Other.ApplyTranslation)
	{}

	FStreamHierarchy(FName InJointName, int32 InParentIndex, FVector InDefaultOffset, bool InApplyTranslation)
		: JointName(InJointName)
		, ParentIndex(InParentIndex)
		, DefaultOffset(InDefaultOffset)
		, ApplyTranslation(InApplyTranslation)
	{}

	bool operator==(const FStreamHierarchy& other) const
	{
		return JointName == other.JointName
			&& ParentIndex == other.ParentIndex
			&& DefaultOffset == other.DefaultOffset
			&& ApplyTranslation == other.ApplyTranslation;
	}

	bool operator!=(const FStreamHierarchy& other) const
	{
		return !(*this == other);
	}

};

class FiPiMocapLiveLinkSource : public ILiveLinkSource, public FRunnable
{
public:

	FiPiMocapLiveLinkSource(FIPv4Endpoint Endpoint);

	virtual ~FiPiMocapLiveLinkSource();

	// Begin ILiveLinkSource Interface
	
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;

	virtual bool IsSourceStillValid() const override;

	virtual bool RequestSourceShutdown() override;

	virtual FText GetSourceType() const override { return SourceType; };
	virtual FText GetSourceMachineName() const override { return SourceMachineName; }
	virtual FText GetSourceStatus() const override { return SourceStatus; }
	// End ILiveLinkSource Interface

	// Begin FRunnable Interface

	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	void Start();
	virtual void Stop() override;
	virtual void Exit() override { }

	// End FRunnable Interface
private:

	ILiveLinkClient* Client;

	// Our identifier in LiveLink
	FGuid SourceGuid;

	FMessageAddress ConnectionAddress;

	FText SourceType;
	FText SourceMachineName;
	FText SourceStatus;

	FIPv4Endpoint DeviceEndpoint;

	// Socket to receive data on
	FSocket* Socket;

	// Subsystem associated to Socket
	ISocketSubsystem* SocketSubsystem;

	// Threadsafe Bool for terminating the main thread loop
	FThreadSafeBool Stopping;

	// Thread to run socket operations on
	FRunnableThread* Thread;

	// Name of the sockets thread
	FString ThreadName;

	// Time to wait between attempted receives
	FTimespan WaitTime;

    // Buffer to receive socket data into
    TArray<uint8> RecvBuffer;

	const char* SIGNATURE = "iPiMocap";
	const int32 SignatureLength;
	const int32 MinPacketSize;
	const int PACKET_KIND_POSE = 0;
	const int PACKET_KIND_HIERARCHY = 2; // we do not use value 1

	FName SubjectName;
	FCriticalSection hierarchyDataLock;
	TArray<FStreamHierarchy> JointsToStream;

	void ReadAndSetSkeletonFrameData(iPiBinaryReader& reader);
	void ReadAndSetAnimationFrameData(iPiBinaryReader& reader);
};