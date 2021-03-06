#ifndef NAN_VERSION
#define NAN_VERSION

#include "node.h"
#include "uv.h"
#include "nan.h"

//// async Fire and Forget call ////

void NanAsyncAddInQueue(uv_async_t *handle, int status);

/**
  * Allow safe exection of a function from another thread
  *
  * NB:The use of this class is for only one call to avoid the problem in uv_async_call:
  * "not every call to it will yield an execution of the callback." see warning in
  * http://docs.libuv.org/en/latest/async.html#c.uv_async_send
  *
  **/
/* abstract */ class NanAsyncLaunch {
 public:
  NanAsyncLaunch(NanCallback* callback):callback(callback),alreadyFired(false) {
    uv_async_init(uv_default_loop(), &(this->async), NanAsyncLaunch::AddInQueue);
    async.data = this;
  }

  void FireAndForget() {
    if(!alreadyFired) {
      alreadyFired=true;
      uv_async_send(&(this->async));
    }
  }
  virtual void Execute() = 0;

  virtual void Destroy() {
    uv_close(reinterpret_cast<uv_handle_t*>(&this->async),NanAsyncLaunch::AsyncClose_);
  }

#if NODE_VERSION_AT_LEAST(0,12,0)
  inline static void AddInQueue(uv_async_t *handle) {
#elif NODE_VERSION_AT_LEAST(0,10,0)
  inline static void AddInQueue(uv_async_t *handle, int status) {
#endif
    NanAsyncLaunch* asyncLaunch = static_cast<NanAsyncLaunch*>(handle->data);
#if NODE_MAJOR_VERSION == 0 && NODE_MINOR_VERSION <= 10
    if(status != UV_ECANCELED)
#endif
      asyncLaunch->Execute();
    asyncLaunch->Destroy();
  }

inline static void AsyncClose_(uv_handle_t* handle) {
      NanAsyncLaunch *launcher =
              static_cast<NanAsyncLaunch*>(handle->data);
      delete launcher;
  }

  virtual ~NanAsyncLaunch(){
    NanScope();
    if (!persistentHandle.IsEmpty())
      NanDisposePersistent(persistentHandle);
    if (callback)
      delete callback;
  }

 protected:
  NanCallback* callback;
  Persistent<Object> persistentHandle;
  static const uint32_t kIndex = 0;
 private:
  uv_async_t async;
  bool alreadyFired;
};

class NoCLMapCB:public NanAsyncLaunch {
 public:
   NoCLMapCB(const v8::Local<v8::Object> &buffer,size_t size,void* mPtr):NanAsyncLaunch(nullptr),size(size),mPtr(mPtr){
       NanScope();
       v8::Local<v8::Object> obj = NanNew<v8::Object>();
       NanAssignPersistent(persistentHandle, obj);
       v8::Local<v8::Object>  handle = NanNew(persistentHandle);
       handle->Set(kIndex, buffer);
   }

   void CallBackIsDone(){
       this->FireAndForget();
   }

   void Execute() {
     NanScope();
     v8::Local<v8::Object> handle = NanNew(persistentHandle);
     v8::Local<v8::Object> buffer= (handle->Get(kIndex)).As<v8::Object>();
     buffer->SetIndexedPropertiesToExternalArrayData(this->mPtr, v8::kExternalByteArray, this->size);
   }

 private:
   size_t size;
   void* mPtr;

};

#endif // NANEXTENSION

