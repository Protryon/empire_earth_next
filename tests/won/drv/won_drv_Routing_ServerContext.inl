// ServerContext's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
typedef std::list<IPAddr> AddrList;
// shuffled the first time (the CRT's srand(time(NULL)) and rand, as random_shuffle drew)
void CopyAddresses(AddrList& outCopy) { outCopy.clear(); ee_won_ctx_copy(this->mList, ee_won_crt_rand, ToList, &outCopy); } //2ac6d0
// cleared and refilled under the list's one lock
bool AtomicClearAndAddAddressesFromDir(const DirList& inDir, const wchar_t* inNameFilter=NULL)
{ return ee_won_ctx_replace_from_dir(this->mList, inDir.get(), inNameFilter, WideLength(inNameFilter), sizeof(wchar_t)); }
// the original's public mShuffleAddresses (copies shuffle once while it is on)
void SetShuffleAddresses(bool inShuffle) { ee_won_ctx_set_shuffle(this->mList, inShuffle); }
static IPAddr FromRaw(const EeWonAddr* inRaw) { IPAddr a; a.FromRaw(*inRaw); return a; }
static void ToList(void* inList, const EeWonAddr* inRaw) { static_cast<AddrList*>(inList)->push_back(FromRaw(inRaw)); }
