/* Private testing adapter only. Not linked into or shipped with the PC port.
 * Uses unmodified mGBA 0.10.5; all save data lives in memory.
 */
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba-util/vfs.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#define API __declspec(dllexport)
static void quiet_log(struct mLogger* l,int cat,enum mLogLevel level,const char* fmt,va_list args) {
    (void)l; (void)cat;
    if(level==mLOG_FATAL || level==mLOG_ERROR) { vfprintf(stderr,fmt,args); fputc('\n',stderr); }
}
static struct mLogger logger={quiet_log,NULL};
struct Probe { struct mCore* core; color_t video[240*160]; };
API struct Probe* probe_open(const char* rom, const char* bios) {
    mLogSetDefaultLogger(&logger);
    struct Probe* p=calloc(1,sizeof(*p)); if(!p) return NULL;
    p->core=mCoreCreate(mPLATFORM_GBA);
    if(!p->core || !p->core->init(p->core)) { free(p); return NULL; }
    mCoreInitConfig(p->core,NULL);
    if(!p->core->loadROM(p->core,VFileOpen(rom,O_RDONLY))) return NULL;
    if(bios && *bios && !p->core->loadBIOS(p->core,VFileOpen(bios,O_RDONLY),0)) return NULL;
    p->core->setVideoBuffer(p->core,p->video,240);
    p->core->opts.skipBios=!(bios && *bios);
    p->core->reset(p->core); return p;
}
API void probe_close(struct Probe* p) { if(p){p->core->deinit(p->core); free(p);} }
API void probe_step(struct Probe* p,unsigned keys,unsigned frames) {
    p->core->setKeys(p->core,keys);
    for(unsigned i=0;i<frames;++i) p->core->runFrame(p->core);
}
API void probe_read(struct Probe* p,unsigned addr,unsigned char* dst,unsigned len) {
    for(unsigned i=0;i<len;++i) dst[i]=p->core->busRead8(p->core,addr+i);
}
API void probe_write(struct Probe* p,unsigned addr,const unsigned char* src,unsigned len) {
    for(unsigned i=0;i<len;++i) p->core->busWrite8(p->core,addr+i,src[i]);
}
API unsigned probe_state_size(struct Probe* p) { return p->core->stateSize(p->core); }
API int probe_save_state(struct Probe* p,void* out) { return p->core->saveState(p->core,out); }
API int probe_load_state(struct Probe* p,const void* in) { return p->core->loadState(p->core,in); }
API int probe_save_import(struct Probe* p,const void* data,unsigned size) {
    return p->core->savedataRestore(p->core,data,size,false);
}
