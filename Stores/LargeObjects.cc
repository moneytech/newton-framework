/*
	File:		LargeObjects.cc

	Contains:	Large object functions.

	Written by:	Newton Research Group.
*/

#define __USERMONITOR_H 1
#include "UserObjects.h"
class CUMonitor
{
public:
					CUMonitor(ObjectId id = 0);
					operator	ObjectId()	const;
	NewtonErr	invokeRoutine(int inSelector, void * ioMsg);
};
inline			CUMonitor::CUMonitor(ObjectId id)  { }
inline			CUMonitor::operator ObjectId()	const  { return 0; }

#include "LargeObjects.h"
#include "LargeBinaries.h"
#include "PackageManager.h"
#include "UStringUtils.h"
#include "ROMResources.h"
#include "OSErrors.h"
#include "RDM.h"


/* -----------------------------------------------------------------------------
	D a t a
----------------------------------------------------------------------------- */

extern Ref	gLBCache;
extern void	PutEntryIntoCache(RefArg inCache, RefArg inEntry);

extern Ref	ToObject(CStore * inStore);


extern CROMDomainManager1K * gROMStoreDomainManager;


#pragma mark -
/*--------------------------------------------------------------------------------
	C U M o n i t o r
	Not the real thing: but allows us to invoke routines in the gROMStoreDomainManager.
--------------------------------------------------------------------------------*/
inline NewtonErr	CUMonitor::invokeRoutine(int inSelector, void * ioMsg)  { return gROMStoreDomainManager->userRequest(inSelector, ioMsg); }
extern CUMonitor *	GetROMDomainUserMonitor(void);


#pragma mark -
/* -----------------------------------------------------------------------------
	L a r g e   O b j e c t s
----------------------------------------------------------------------------- */

NewtonErr
CreateLargeObject(PSSId * outId, CStore * inStore, size_t inSize, const char * inCompanderName, void * inCompanderParms, size_t inCompanderParmSize)
{
	return CreateLargeObject(outId, inStore, NULL, inSize, false, inCompanderName, inCompanderParms, inCompanderParmSize, NULL, false);
}


NewtonErr
CreateLargeObject(PSSId * outId, CStore * inStore, CPipe * inPipe, size_t inSize, bool inReadOnly, const char * inCompanderName, void * inCompanderParms, size_t inCompanderParmSize, CLOCallback * inCallback, bool inCompressed)
{
	NewtonErr	err;

	XTRY
	{
#if defined(correct)
		if (inPipe)
			XFAIL(err = gAppWorld->fork(NULL))
#endif
		CLrgObjStore * objStore = (CLrgObjStore *)MakeByName("CLrgObjStore", NULL, inCompanderName);
		if (objStore != NULL)
		{
			XFAILIF(err = objStore->init(), objStore->destroy();)
			if (inCompressed)
				XFAIL(err = objStore->createFromCompressed(outId, inStore, inPipe, inSize, inReadOnly, inCompanderName, inCompanderParms, inCompanderParmSize, inCallback))
			else
				XFAIL(err = objStore->create(outId, inStore, inPipe, inSize, inReadOnly, inCompanderName, inCompanderParms, inCompanderParmSize, inCallback))
			objStore->destroy();
		}
		else
		{
			XFAILNOT(ClassInfoByName("CStoreCompander", inCompanderName), err = kOSErrNoMemory;)
			if (inCompressed)
				XFAIL(err = LODefaultCreateFromCompressed(outId, inStore, inPipe, inSize, inReadOnly, inCompanderName, inCompanderParms, inCompanderParmSize, inCallback))
			else
				XFAIL(err = LODefaultCreate(outId, inStore, inPipe, inSize, inReadOnly, inCompanderName, inCompanderParms, inCompanderParmSize, inCallback))
		}
	}
	XENDTRY;

	return err;
}


NewtonErr
DuplicateLargeObject(PSSId * outId, CStore * inStore, PSSId inId, CStore * intoStore)
{
	NewtonErr		err;
	CLrgObjStore *	objStore = NULL;

	XTRY
	{
		XFAIL(err = GetLOAllocator(inStore, inId, &objStore))
		if (objStore != NULL)
		{
			err = objStore->duplicate(outId, inStore, inId, intoStore);
			objStore->destroy();
		}
		else
			err = LODefaultDuplicate(outId, inStore, inId, intoStore);
	}
	XENDTRY;

	return err;
}


/* -----------------------------------------------------------------------------
	Map a large object to a virtual address.
	The object is paged into memory from backing store when memory is faulted.
	Args:		outAddr		on return, the virtual address of the large object
				inStore		store for the object
				inId			id of the object on store
				inReadOnly	true => give the mapped memory RO permission
	Return:	error code
----------------------------------------------------------------------------- */

NewtonErr
MapLargeObject(VAddr * outAddr, CStore * inStore, PSSId inId, bool inReadOnly)
{
	if (!PackageAllocationOK(inStore, inId))
		return kOSErrBadObject;

	NewtonErr	err;
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fStore = inStore;
	parms.fObjId = inId;
	parms.fRO = inReadOnly;
	if ((err = um.invokeRoutine(kRDM_MapLargeObject, &parms)) == noErr)
		*outAddr = parms.fAddr;
	return err;
}


NewtonErr
UnmapLargeObject(CStore ** outStore, PSSId * outId, VAddr inAddr)
{
	NewtonErr	err;
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fAddr = inAddr;
	err = um.invokeRoutine(kRDM_UnmapLargeObject, &parms);
	*outStore = parms.fStore;
	*outId = parms.fObjId;
	return err;
}


NewtonErr
UnmapLargeObject(VAddr inAddr)
{
	CStore *	store;
	PSSId		id;
	return UnmapLargeObject(&store, &id, inAddr);
}


NewtonErr
FlushLargeObject(CStore * inStore, PSSId inId)
{
	NewtonErr	err;
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fStore = inStore;
	parms.fObjId = inId;
	parms.fPkgId = 0;
	err = um.invokeRoutine(kRDM_FlushLargeObject, &parms);
	if (err == kOSErrNoSuchPackage)
		err = noErr;
	return err;
}


NewtonErr
ResizeLargeObject(VAddr * outAddr, VAddr inAddr, size_t inLength, long inArg4)
{
	NewtonErr	err;
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fAddr = inAddr;
	parms.fSize = inLength;
	parms.f14 = inArg4;
	err = um.invokeRoutine(kRDM_ResizeLargeObject, &parms);
	*outAddr = parms.fAddr;
	return err;
}


NewtonErr
AbortObject(CStore * inStore, PSSId inId)
{
	NewtonErr	err;
	VAddr			addr;
	if ((err = StoreToVAddr(&addr, inStore, inId)) == noErr)
	{
		RDMParams	parms;
		CUMonitor	um(*GetROMDomainUserMonitor());
		parms.fAddr = addr;
		err = um.invokeRoutine(kRDM_AbortObject, &parms);
	}
	else if (inStore->inSeparateTransaction(inId))
		err = LODefaultDoTransaction(inStore, inId, 0, 1, true);
	return err;
}


NewtonErr
AbortObjects(CStore * inStore)
{
	NewtonErr	err;
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fStore = inStore;
	err = um.invokeRoutine(kRDM_AbortObject, &parms);
	return err;
}


NewtonErr
CommitObject(VAddr inAddr)
{
	NewtonErr	err;
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fAddr = inAddr;
	err = um.invokeRoutine(kRDM_CommitObject, &parms);
	return err;
}

NewtonErr
CommitObjects(CStore * inStore)
{
	NewtonErr	err;
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fStore = inStore;
	err = um.invokeRoutine(kRDM_CommitObject, &parms);
	return err;
}


NewtonErr
GetLargeObjectInfo(RDMParams * outParms, VAddr inAddr)
{
	CUMonitor	um(*GetROMDomainUserMonitor());
	outParms->fAddr = inAddr;
	return um.invokeRoutine(kRDM_GetLargeObjectInfo, outParms);
}


bool
LargeObjectIsDirty(VAddr inAddr)
{
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fAddr = inAddr;
	return um.invokeRoutine(kRDM_GetLargeObjectInfo, &parms) == noErr
		 && parms.fDirty;
}


bool
LargeObjectIsReadOnly(VAddr inAddr)
{
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fAddr = inAddr;
	return um.invokeRoutine(kRDM_GetLargeObjectInfo, &parms) != noErr
		 || parms.fRO;
}


size_t
ObjectSize(VAddr inAddr)
{
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fAddr = inAddr;
	parms.fSize = 0;
	XTRY
	{
		XFAIL(um.invokeRoutine(kRDM_GetLargeObjectInfo, &parms))
#if 0
		if (IsPackageHeader(inAddr, sizeof(PackageDirectory)))
		{
			CPackageIterator	iter(inAddr);
			XFAIL(iter.init)
			return iter.packageSize();
		}
#endif
	}
	XENDTRY;
	return parms.fSize;
}


NewtonErr
VAddrToStore(CStore ** outStore, PSSId * outId, VAddr inAddr)
{
	NewtonErr	err;
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fAddr = inAddr;
	err = um.invokeRoutine(kRDM_GetLargeObjectInfo, &parms);
	*outStore = parms.fStore;
	*outId = parms.fObjId;
	return err;
}


NewtonErr
StoreToVAddr(VAddr * outAddr, CStore * inStore, PSSId inId)
{
	NewtonErr	err;
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fStore = inStore;
	parms.fObjId = inId;
	err = um.invokeRoutine(kRDM_StoreToVAddr, &parms);
	*outAddr = parms.fAddr;
	return err;
}


bool
LargeObjectAddressIsValid(VAddr inAddr)
{
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fAddr = inAddr;
	return um.invokeRoutine(kRDM_LargeObjectAddress, &parms) == noErr
		 && IsValidStore(parms.fStore);	// calls CPSSManager
}


size_t
StorageSizeOfLargeObject(VAddr inAddr)
{
	CStore *		store;
	PSSId			id;
	if (VAddrToStore(&store, &id, inAddr) == noErr)
		return StorageSizeOfLargeObject(store, id);
	return 0;
}

size_t
StorageSizeOfLargeObject(CStore * inStore, PSSId inId)
{
	NewtonErr		err;
	size_t			size = 0;
	CLrgObjStore *	objStore = NULL;

	XTRY
	{
		XFAIL(err = FlushLargeObject(inStore, inId))
		XFAIL(err = GetLOAllocator(inStore, inId, &objStore))
		if (objStore != NULL)
			err = objStore->storageSize(inStore, inId);
		else
			err = LODefaultStorageSize(inStore, inId);
	}
	XENDTRY;

	if (objStore)
		objStore->destroy();

	XDOFAIL(err)
	{
		ThrowErr(exAbort, err);
	}
	XENDFAIL;

	return size;
}


bool
IsOnStoreAsPackage(VAddr inAddr)
{
	CStore *		store;
	PSSId			id;
	if (VAddrToStore(&store, &id, inAddr) == noErr)
		return IsOnStoreAsPackage(store, id);
	return false;
}

bool
IsOnStoreAsPackage(CStore * inStore, PSSId inId)
{
	bool isSo = false;

	XTRY
	{
		int	objType;
		PackageRoot	root;
		XFAIL(inStore->read(inId, 0, &root, sizeof(root)))
		objType = root.flags & 0xFFFF;
		XFAIL(objType > 2)
		isSo = (objType == 1 || (root.flags & 0x00020000) != 0);
	}
	XENDTRY;

	return isSo;
}


Ref
WrapLargeObject(CStore * inStore, PSSId inId, RefArg inClass, VAddr inAddr)
{
	RefVar	storeObject(ToObject(inStore));
	if (ISNIL(storeObject))
		ThrowOSErr(kNSErrInvalidStore);

	CStoreWrapper *	storeWrapper = (CStoreWrapper *)GetFrameSlot(storeObject, SYMA(store));
	if (ISNIL(storeWrapper))	// orginal checks storeWrapper == NULL
		ThrowOSErr(kNSErrOldStoreFormat);

	CheckWriteProtect(storeWrapper->store());
	storeWrapper->addEphemeral(inId);
	size_t	size = (inAddr == 0) ? 0 : ObjectSize(inAddr);
	StoreRef	classRef = storeWrapper->symbolToReference(inClass);

	RefVar	obj(AllocateLargeBinary(inClass, classRef, size, storeWrapper));
	IndirectBinaryObject *  objPtr = (IndirectBinaryObject *)ObjectPtr(obj);
	LBData * largeBinary = (LBData *)&objPtr->data;
	largeBinary->fId = inId;
	largeBinary->fClass = classRef;
	largeBinary->fAddr = inAddr;

	PutEntryIntoCache(gLBCache, obj);
	return obj;
}


Ref
GetEntryFromLargeObjectVAddr(VAddr inAddr)
{ return NILREF; }


NewtonErr
DeleteLargeObject(CStore * inStore, PSSId inId)
{
	NewtonErr	err;
	VAddr			addr;
	if ((err = StoreToVAddr(&addr, inStore, inId)) != noErr
	||  (err = UnmapLargeObject(addr)) == noErr)
		err = LODeleteByProtocol(inStore, inId);
	return err;
}


NewtonErr
DeleteLargeObject(VAddr inAddr)
{
	NewtonErr	err;
	CStore *		store;
	PSSId			objId;
	if ((err = UnmapLargeObject(&store, &objId, inAddr)) == noErr)
		err = DeallocatePackage(store, objId);
	return err;
}


#pragma mark -
/*------------------------------------------------------------------------------
	P a c k a g e s
------------------------------------------------------------------------------*/
const UniChar * g0C1016E4 = (const UniChar *)L"Patch";		// I think; it’s not defined in the original

NewtonErr	InstallPackage(void*, SourceType, ULong*, bool*, bool*, CStore * inStore = NULL, ObjectId inId = kNoId);


size_t
SizeOfPatches(void)
{
#if !defined(forFramework)
	size_t patchPageSize;
	size_t patchSize = 0xD8 + (Ustrlen(g0C1016E4)+1)*sizeof(UniChar);

	GestaltPatchInfo info;
	CUGestalt gestalt;
	gestalt.gestalt(kGestalt_PatchInfo, &info, sizeof(GestaltPatchInfo));
	patchPageSize = info.fTotalPatchPageCount * kPageSize;
	// first entry is base ROM patch
	// remaining 4 entries are ROM extensions
	for (ArrayIndex i = 1; i < 5; ++i)
	{
		if (info.fPatch[i].fPatchPageCount > 0)
			patchPageSize += kPageSize;
	}
	
	return (patchPageSize != 0) ? patchSize + patchPageSize : 0;
#else
	return 0;
#endif
}


NewtonErr
BackupPatches(CPipe * inPipe)
{ return noErr; }


NewtonErr
NewPackage(CPipe * inPipe, CStore * inStore, PSSId inId, ULong * outArg4, const char * inCompanderName, void * inCompanderParms, size_t inCompanderParmSize, CCallbackCompressor * inCompressor)
{
// r0 r4 r5 r6 r9 r10 r8 r7
	NewtonErr err;
	bool sp00 = false;	// isSystemPatch?
	inStore->lockStore();	// sic -- ignoring any error
	err = AllocatePackage(inPipe, inStore, inId, inCompanderName, inCompanderParms, inCompanderParmSize,  inCompressor, NULL);
	if (err == noErr)
	{
		XTRY
		{
			bool spm04;
			XFAILIF(err = PackageAvailable(inStore, inId, outArg4, &sp00, &spm04), DeallocatePackage(inStore, inId);)
			XTRY
			{
				XFAILIF(sp00, *outArg4 = 0; DeallocatePackage(inStore, inId);)	// not really an error
				VAddr addr;
				XFAILIF(MapLargeObject(&addr, inStore, inId, true), DeallocatePackage(inStore, inId);)	// sic -- don’t record the err
				XFAILIF(CommitObject(addr), DeallocatePackage(inStore, inId);)
				UnmapLargeObject(addr);
			}
			XENDTRY;
			err = inStore->unlockStore();
#if !defined(forFramework)
			if (sp00)
				Reboot(kOSErrNewSystemSoftware);	// we just installed a system patch
#endif
		}
		XENDTRY;
	}
	else if (err == 1)
	{
		// system patch was loaded
		err = noErr;
		*outArg4 = 0;
	}
	else
		// could not allocate package
		inStore->abort();
	
	return err;
}


NewtonErr
AllocatePackage(CPipe * inPipe, CStore * inStore, PSSId inId, const char * inCompanderName, void * inCompanderParms, size_t inCompanderParmSize, CCallbackCompressor * inCompressor, CLOCallback * inCallback)
{
	NewtonErr err = noErr;
#if 0
	PackageRoot root;
	CPackageIterator iter(inPipe);	// we just use the iterator to get information about the package
	XTRY
	{
		XFAIL(err = iter.init)
		if (Ustrcmp(iter.packageName(), g0C1016E4) == 0)	// system patch name?
		{
			RefVar pkgRef(AllocateBinary(SYMA(binary), iter.packageSize()));
			CDataPtr pkgData(pkgRef);
			Ptr p = (char *)pkgData;
			// copy package directory
			memcpy(p, iter.x08, sizeof(PackageDirectory));
			// copy all part entries after that
			size_t partInfoSize = sizeof(PartEntry) * iter.numberOfParts();
			memcpy(p + sizeof(PackageDirectory), iter.x0C, partInfoSize);
			// copy package info after that
			memcpy(p + sizeof(PackageDirectory) + partInfoSize, iter.x18, iter.directorySize() - partInfoSize);
			// copy package data after that
			newton_try
			{
				bool isEOF = false;
				size_t pkgDataSize = iter.packageSize() - iter.directorySize();
				inPipe->readChunk(p + iter.directorySize(), pkgDataSize, isEOF);
			}
			newton_catch(exPipe)
			{
				err = (NewtonErr)(long)CurrentException()data;
			}
			end_try;
			XFAIL(err)

			PSSId pkgId;
			SourceType source;
			source.format = kFixedMemory;
			err = LoadPackage(p, source, &pkgId);
			if (!err)
				err = 1;
			inStore->abort();
		}
		else
		{
			XFAIL(err = inStore->setObjectSize(inId, sizeof(PackageRoot)))
			XFAIL(err = inStore->newWithinTransaction(0, &root.fDataId))
			if ((iter.packageFlags() & 0x10000000) != 0)
			{
				inCompanderName = (iter.packageFlags() & 0x04000000) != 0 ? "CSimpleRelocStoreDecompressor" : "CSimpleStoreDecompressor";
				inCompanderParmSize = 0;
			}
			// write compander name
			XFAIL(err = inStore->newWithinTransaction(strlen(inCompanderName), &root.fCompanderNameId))
			XFAIL(err = inStore->write(root.fCompanderNameId, 0, inCompanderName, strlen(inCompanderName)))
			// write compander parms
			XFAIL(err = inStore->newWithinTransaction(inCompanderParmSize, &root.fCompanderParmsId))
			if (inCompanderParmSize > 0)
				XFAIL(err = inStore->write(root.fCompanderParmsId, 0, inCompanderParms, inCompanderParmSize))
			// set up store
			XFAIL(err = iter.store(inStore, root.fDataId, inCompressor, inCallback))
			// update package root => OK
			root.fSignature = 'paok';
			err = inStore->write(inId, 0, root.fDataId, sizeof(PackageRoot));
		}
	}
	XENDTRY;
	XDOFAIL(err < 0)
	{
		if (root.fCompanderParmsId != kNoId)
			inStore->separatelyAbort(root.fCompanderParmsId);
		if (root.fCompanderNameId != kNoId)
			inStore->separatelyAbort(root.fCompanderNameId);
		if (root.fDataId != kNoId)
			inStore->separatelyAbort(root.fDataId);
	}
	XENDFAIL;
#endif
	return err;
}

/* no need for this if we use default argument for inCompressor=NULL
NewtonErr
AllocatePackage(CPipe * inPipe, CStore * inStore, PSSId inId, const char * inCompanderName, void * inCompanderParms, size_t inCompanderParmSize, CCallbackCompressor * inCompressor)
{ return AllocatePackage(inPipe, inStore, inId, inCompanderName, inCompanderParms, inCompanderParmSize, NULL); }*/


bool
PackageAllocationOK(CStore * inStore, PSSId inId)
{
	PackageRoot root;
	return inStore->read(inId, 0, &root, sizeof(root)) == noErr
		 && root.fSignature == 'paok';
}


NewtonErr
PackageAvailable(CStore * inStore, PSSId inId, ULong * outArg3, bool * outArg4, bool * outArg5)
{
	if (!PackageAllocationOK(inStore, inId))
		return kOSErrBadObject;
#if 0
	NewtonErr err;
	XTRY
	{
		RDMParams	parms;
		CUMonitor	um(GetROMDomainManagerId());
		parms.fStore = inStore;
		parms.fObjId = inId;
		parms.fPkgId = kNoId;
		parms.fRO = true;
		parms.fDirty = false;
		XFAIL(err = um.invokeRoutine(kRDM_MapLargeObject, &parms))

		SourceType	source;
		source.format = kRemovableMemory;
		source.deviceKind = kStoreDevice;
		source.deviceId = 0;
		XFAIL(err = InstallPackage((void *)parms.fAddr, &source, outArg3, outArg4, outArg5, parms.fStore, parms.fObjId))
		um.invokeRoutine(kRDM_UnmapLargeObject, &parms);
	}
	XENDTRY;
	return err;

#else
	return noErr;
#endif
}


NewtonErr
PackageAvailable(CStore * inStore, PSSId inId, ULong * outArg3)
{
	NewtonErr err;
	bool sp00 = false;
	bool sp04 = false;
	err = PackageAvailable(inStore, inId, outArg3, &sp00, &sp04);
	if (err == noErr)
	{
		if (sp00)
			*outArg3 = 0;
#if !defined(forFramework)
		if (sp04)
			Reboot(kOSErrNewSystemSoftware);
#endif
	}
	return err;
}


NewtonErr
PackageUnavailable(PSSId inPkgId)
{
#if 0
	StopFrameSound();

	NewtonErr	err;
	RDMParams	parms;
	CUMonitor	um(GetROMDomainManagerId());
	parms.fStore = NULL;
	parms.fObjId = 0;
	parms.fPkgId = inPkgId;
	um.invokeRoutine(kRDM_FlushLargeObject, &parms);

	err = DeinstallPackage(inPkgId);

	parms.fStore = NULL;
	parms.fObjId = 0;
	parms.fPkgId = inPkgId;
	um.invokeRoutine(kRDM_UnmapLargeObject, &parms);
	
	return err;

#else
	return noErr;
#endif
}


/* -----------------------------------------------------------------------------
	Install package.
----------------------------------------------------------------------------- */

NewtonErr
InstallPackage(void * inPkgData, SourceType inSrcType, PSSId * outPkgId, bool * outArg4, bool * outArg5, CStore * inStore, ObjectId inId)
{
//r8 r1 r2 r4 r9 r7 r6 r10
#if !defined(forFramework)
	NewtonErr err;
	if ((err = gPkgWorld->fork(NULL)) == noErr) {
		PartSource src;
		src.mem.buffer = inPkgData;
		CPkBeginLoadEvent loadEvent(inSrcType, src, *gPkgWorld->getMyPort(), *gPkgWorld->getMyPort(), true);			//sp10
#if 0
		CUMonitor	um(GetROMDomainManagerId());	//sp08
		CUPort pkgMgr(PackageManagerPortId());		//sp00
		size_t replySize;

		gPkgWorld->releaseMutex();
		gPackageSemaphore->acquire(kWaitOnBlock);
		
		err = pkgMgr.sendRPC(&replySize, &loadEvent, sizeof(CPkBeginLoadEvent), &loadEvent, sizeof(CPkBeginLoadEvent));
		if (err == noErr
		&&  inStore != NULL
		&& !loadEvent.f81) {
			RDMParams	parms;
			parms.fStore = inStore;
			parms.fObjId = inId;
			parms.fPkgId = loadEvent.fUniqueId;
			err = um.invokeRoutine(kRDM_4, &parms);	// set package id
		}

		gPackageSemaphore->release();
		gPkgWorld->acquireMutex();

		*outPkgId = loadEvent.f81 ? 0 : loadEvent.fUniqueId;
		if (outArg4)
			*outArg4 = loadEvent.f81;
		if (outArg5)
			*outArg5 = loadEvent.f80;
#endif
	}
	return err;
#else
	return noErr;
#endif
}

/* no need for this if we use default arguments
NewtonErr
InstallPackage(void * inPkgData, SourceType inSrcType, PSSId * outPkgId, bool*, bool*)
{ return noErr; }*/


NewtonErr
BackupPackage(CPipe * inPipe, PSSId inId)
{
	NewtonErr err = noErr;

	PackageRoot root;
	CStoreDecompressor * compander = NULL;
	char * companderBuffer = NULL;
	char * companderName = NULL;
	char * lzwBuffer = NULL;

	XTRY
	{
//		all these NewPtr()s were originally performed by:
//		x = new char[KByte];	XFAIL(err = MemError())
		companderBuffer = NewPtr(KByte);
		XFAILIF(companderBuffer == NULL, err = kOSErrNoMemory;)

		// locate package in memory
		VAddr pkgAddr;
		XFAIL(err = IdToVAddr(inId, &pkgAddr))

		// fetch its uncompressed size
		size_t pkgSize;
#if !defined(forFramework)
		CPackageIterator iter((void *)pkgAddr);
		XFAIL(err = iter.init())
		pkgSize = iter.packageSize();
#endif

		//sp-04
		int sp04 = 0;
		CStore * store;
		PSSId objId;
		XFAIL(err = IdToStore(inId, &store, &objId))

		// fetch the package root
		XFAIL(err = store->read(objId, 0, &root, sizeof(PackageRoot)))
		XFAILNOT(root.flags == 1, err = kOSErrBadPackage;)	// objType == package?

		// create the decompressor
		size_t objSize;
		XFAIL(err = store->getObjectSize(root.fCompanderNameId, &objSize))

		XFAILNOT(companderName = NewPtr(objSize+1), err = kOSErrNoMemory;)
		XFAIL(err = store->read(root.fCompanderNameId, 0, companderName, objSize))
		companderName[objSize] = 0;

		XFAILNOT(compander = (CStoreDecompressor *)MakeByName("CStoreDecompressor", companderName), err = kOSErrNoMemory;)
		if (strcmp(companderName, "CLZStoreDecompressor") == 0 || strcmp(companderName, "CLZRelocStoreDecompressor") == 0)
		{
			// LZ decompressors need a buffer as parameter
			XFAILNOT(lzwBuffer = NewPtr(0x520), err = kOSErrNoMemory;)
			XFAIL(err = compander->init(store, kNoId, lzwBuffer))
		}
		else
			XFAIL(err = compander->init(store, root.fCompanderParmsId))

		// write the data
		size_t numOfChunks;
		XFAIL(err = store->getObjectSize(root.fDataId, &numOfChunks))
		// convert size of array to number of entries
		numOfChunks /= sizeof(PSSId);

		// write package to the pipe, a chunk at a a time
		size_t	chunkSize;
		size_t	sizeDone = 0;
		for (ArrayIndex chunkIndex = 0; chunkIndex < numOfChunks; chunkIndex++, sizeDone += chunkSize)
		{
			PSSId chunkId;
			XFAIL(err = store->read(root.fDataId, chunkIndex*sizeof(PSSId), &chunkId, sizeof(PSSId)))
			XFAIL(err = compander->read(chunkId, companderBuffer, (size_t)KByte, (VAddr)0))

			chunkSize = pkgSize - sizeDone;
			if (chunkSize > KByte)
				chunkSize = KByte;
			newton_try
			{
				inPipe->writeChunk(companderBuffer, chunkSize, false);
			}
			newton_catch(exPipe)
			{
				err = (NewtonErr)(long)CurrentException()->data;;
			}
			end_try;
			XFAIL(err)
		}
	}
	XENDTRY;

	if (compander)
		compander->destroy();
	if (companderBuffer)
		FreePtr(companderBuffer);
	if (companderName)
		FreePtr(companderName);
	if (lzwBuffer)
		FreePtr(lzwBuffer);

	return err;
}


NewtonErr
BackupPackage(CPipe * inPipe, CStore * inStore, PSSId inId, CLOCallback * inCallback)
{
	NewtonErr err = noErr;

	PackageRoot root;
	CStoreDecompressor * compander = NULL;
	char * companderBuffer = NULL;
	char * companderName = NULL;
	char * lzwBuffer = NULL;

	XTRY
	{
//		all these NewPtr()s were originally performed by:
//		x = new char[KByte];	XFAIL(err = MemError())
		companderBuffer = NewPtr(KByte);
		XFAILIF(companderBuffer == NULL, err = kOSErrNoMemory;)

		// map the package into memory
		VAddr pkgAddr;
		XFAIL(err = StoreToVAddr(&pkgAddr, inStore, inId))
		XFAIL(err = MapLargeObject(&pkgAddr, inStore, inId, false))

		// fetch its uncompressed size
		size_t pkgSize;
#if !defined(forFramework)
		CPackageIterator iter((void *)pkgAddr);
		XFAIL(err = iter.init())
		pkgSize = iter.packageSize();
#endif

		// fetch the package root
		XFAIL(err = inStore->read(inId, 0, &root, sizeof(PackageRoot)))
		XFAILNOT(root.flags == 1, err = kOSErrBadPackage;)	// objType == package?

		// create the decompressor
		size_t objSize;
		XFAIL(err = inStore->getObjectSize(root.fCompanderNameId, &objSize))

		XFAILNOT(companderName = NewPtr(objSize+1), err = kOSErrNoMemory;)
		XFAIL(err = inStore->read(root.fCompanderNameId, 0, companderName, objSize))
		companderName[objSize] = 0;

		XFAILNOT(compander = (CStoreDecompressor *)MakeByName("CStoreDecompressor", companderName), err = kOSErrNoMemory;)
		if (strcmp(companderName, "CLZStoreDecompressor") == 0 || strcmp(companderName, "CLZRelocStoreDecompressor") == 0)
		{
			// LZ decompressors need a buffer as parameter
			XFAILNOT(lzwBuffer = NewPtr(0x520), err = kOSErrNoMemory;)
			XFAIL(err = compander->init(inStore, kNoId, lzwBuffer))
		}
		else
			XFAIL(err = compander->init(inStore, root.fCompanderParmsId))

		// write the data
		size_t numOfChunks;
		XFAIL(err = inStore->getObjectSize(root.fDataId, &numOfChunks))
		// convert size of array to number of entries
		numOfChunks /= sizeof(PSSId);

		// write package to the pipe, a chunk at a a time
		size_t	chunkSize;
		size_t	sizeDone = 0;
		for (ArrayIndex chunkIndex = 0; chunkIndex < numOfChunks; chunkIndex++, sizeDone += chunkSize)
		{
			PSSId chunkId;
			XFAIL(err = inStore->read(root.fDataId, chunkIndex*sizeof(PSSId), &chunkId, sizeof(PSSId)))
			XFAIL(err = compander->read(chunkId, companderBuffer, (size_t)KByte, (VAddr)0))

			chunkSize = pkgSize - sizeDone;
			if (chunkSize > KByte)
				chunkSize = KByte;
			newton_try
			{
				inPipe->writeChunk(companderBuffer, chunkSize, false);
			}
			newton_catch(exPipe)
			{
				err = (NewtonErr)(long)CurrentException()->data;;
			}
			end_try;
			XFAIL(err)
		}
	}
	XENDTRY;

	if (compander)
		compander->destroy();
	if (companderBuffer)
		FreePtr(companderBuffer);
	if (companderName)
		FreePtr(companderName);
	if (lzwBuffer)
		FreePtr(lzwBuffer);

	return err;
}


NewtonErr
DeletePackage(PSSId inPkgId)
{
#if 0
	NewtonErr	err;
	CStore *		theStore;
	PSSId			theId;
	if ((err = IdToStore(inPkgId, &theStore, &theId)) == noErr)
	{
		store->lockStore();
		DeallocatePackage(theStore, theId);
		store->unlockStore();
	}
	else
		DeinstallPackage(inPkgId);
	return err;

#else
	return noErr;
#endif
}


NewtonErr
DeallocatePackage(CStore * inStore, PSSId inId)
{
#if 0
	NewtonErr	err;
	NewtonErr	xerr;
	PackageRoot	root;
	PSSId			pkgId;

	if (StoreToId(inStore, inId, &pkgId) == noErr)
		PackageUnavailable(pkgId);

	XTRY
	{
		XFAIL(err = inStore->read(inId, 0, &root, sizeof(root)))
		XFAILIF((root.flags & 0xFFFF) > 2, err = kOSErrBadPackage;)
		if (root.fDataId != 0)
			err = RemoveIndexTable(inStore, root.fDataId);
		xerr = inStore->deleteObject(root.fCompanderNameId);
		if (err == noErr)
			err = xerr;
		xerr = inStore->deleteObject(root.fCompanderParmsId);
		if (err == noErr)
			err = xerr;
		xerr = inStore->deleteObject(inId);
		if (err == noErr)
			err = xerr;
	}
	XENDTRY;

	return err;

#else
	return noErr;
#endif
}

NewtonErr
SafeToDeactivatePackage(PSSId inId, bool * outArg2)
{
#if 0
	CPkSafeToDeactivateEvent deactivationEvent(inId);
	CUPort pkgMgr(PackageManagerPortId());
	size_t replySize;

	NewtonErr err = pkgMgr.sendRPC(&replySize, &deactivationEvent, sizeof(deactivationEvent), &deactivationEvent, sizeof(deactivationEvent));
	if (!err)
		*outArg2 = deactivationEvent.f14;
	return err;

#else
	return noErr;
#endif
}




Ref
WrapPackage(CStore * inStore, PSSId inId)
{
	NewtonErr	err;
	RefVar		pkgRef;
	VAddr			addr = 0;
	if ((err = MapLargeObject(&addr, inStore, inId, true)) == noErr)
	{
		newton_try
		{
			pkgRef = WrapLargeObject(inStore, inId, SYMA(package), addr);
		}
		newton_catch_all
		{
			err = (NewtonErr)(long)CurrentException()->data;;
		}
		end_try;
	}
	if (err)
	{
		if (ISNIL(pkgRef))
			AbortObject(inStore, inId);
		ThrowErr(exFrames, err);
	}

	return pkgRef;
}


NewtonErr
StorePackage(CPipe * inPipe, CStore * inStore, CLOCallback * inCallback, PSSId * outId)
{
	NewtonErr	err = noErr;
#if 0
	CPackagePipe	pipe;

	newton_try
	{
		pipe.init(inPipe);
	}
	newton_catch(exPipe)
	{
		err = (NewtonErr)(long)CurrentException()data;
	}
	end_try;

	if (err == noErr)
	{
		const char *	compressorName;
		if (pipe.f08->packageFlags() & 0x10000000)  // CPackageIterator
			compressorName = (pipe.f08->packageFlags() & 0x04000000) ? "CSimpleRelocStoreDecompressor" : "CSimpleStoreDecompressor";
		else if (pipe.f08->packageFlags() & 0x02000000)
			compressorName = (pipe.f08->packageFlags() & 0x04000000) ? "CZippyRelocStoreDecompressor" : "CZippyStoreDecompressor";
		else if (pipe.f08->packageFlags() & 0x04000000)
			compressorName = "CLZRelocStoreDecompressor";
		if (pipe.f08->packageFlags() & 0x08000000)
			compressorName = "CXIPStoreCompander";
		err = CreateLargeObject(outId, inStore, &pipe, 0, true, compressorName, NULL, 0, inCallback, false);
	}
#endif
	return err;
}


void
FlushPackageCache(VAddr inAddr)
{
#if 0
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms->fAddr = inAddr;
	um.invokeRoutine(kRDM_FlushLargeObject, &parms);
	ICacheClear();
#endif
}


NewtonErr
IdToStore(PSSId inPkgId, CStore ** outStoreObj, PSSId * outStoreId)
{
#if 0
	NewtonErr	err;
	RDMParams	parms;
	CUMonitor	um(GetROMDomainManagerId());

	parms.fStore = NULL;
	parms.fObjId = 0;
	parms.fPkgId = inPkgId;
	um.invokeRoutine(kRDM_IdToStore, &parms);

	*outStoreObj = parms.fStore;
	*outStoreId = parms.fObjId;
	return err;

#else
	return noErr;
#endif
}

NewtonErr
StoreToId(CStore * inStore, PSSId inId, PSSId * outId)
{ return noErr; }

NewtonErr
IdToVAddr(PSSId inId, VAddr * outAddr)
{ return noErr; }

void
VAddrToId(PSSId * outId, VAddr inAddr)
{
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fAddr = inAddr;
	*outId = (um.invokeRoutine(kRDM_LargeObjectAddress, &parms) == noErr) ? parms.fPkgId : kNoId;
}


void
VAddrToBase(VAddr * outBase, VAddr inAddr)
{
	RDMParams	parms;
	CUMonitor	um(*GetROMDomainUserMonitor());
	parms.fAddr = inAddr;
	*outBase = (um.invokeRoutine(kRDM_LargeObjectAddress, &parms) == noErr) ? parms.fAddr : 0;
}


#pragma mark -
/* -----------------------------------------------------------------------------
	P l a i n   C   I n t e r f a c e
----------------------------------------------------------------------------- */
#include "Globals.h"
#include "ROMResources.h"

extern NewtonErr	GetLargeObjectInfo(RDMParams * outParms, VAddr inAddr);
extern ObjectId	GetROMDomainManagerId(void);


extern "C" {
Ref	FInstallPackage(RefArg inRcvr, RefArg inPkg);
Ref	FDeinstallPackage(RefArg inRcvr, RefArg inPkg);
}


Ref
FInstallPackage(RefArg inRcvr, RefArg inPkg)
{
	NewtonErr err;
	Ptr			pkgData = BinaryData(inPkg);
	PSSId			pkgId;
	RDMParams	parms;
	CUMonitor	um(GetROMDomainManagerId());

	GetLargeObjectInfo(&parms, (VAddr)pkgData);

	SourceType source;
	source.format = kRemovableMemory;
	source.deviceKind = kStoreDeviceV2;
	source.deviceId = 0;
	bool sp08, sp0C;

	if ((err = InstallPackage(pkgData, source, &pkgId, &sp0C, &sp08, parms.fStore, parms.fObjId)) != noErr)
		ThrowErr(exFrames, err);

	if (!sp0C)
	{
		RefVar activePackages(GetGlobalVar(SYMA(activePackageList)));
		AddArraySlot(activePackages, inPkg);
	}

	return MAKEINT(pkgId);
}


Ref
FDeinstallPackage(RefArg inRcvr, RefArg inPkg)
{ return NILREF; }

