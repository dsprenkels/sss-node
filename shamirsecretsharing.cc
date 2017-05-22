#include <node.h>
#include <nan.h>

extern "C" {
  #include "sss/sss.h"
  #include "sss/serialize.h"
}


class CreateSharesWorker : public Nan::AsyncWorker {
 public:
  CreateSharesWorker(char* data, uint8_t n, uint8_t k, char* random_bytes,
                     Nan::Callback *callback)
      : AsyncWorker(callback), n(n), k(n) {
    Nan::HandleScope scope;

    memcpy(this->data, data, sss_MLEN);
    memcpy(this->random_bytes, random_bytes, 32);
    size_t output_size = n * sss_SHARE_SERIALIZED_LEN;
    if (output_size / sss_SHARE_SERIALIZED_LEN != n) {
        // Overflow occurred! Is an unreachable state, because `n` is
        // between 0 and 256, and `sss_CLEN` is a small static value (80).
        Nan::ThrowError("unreachable state");
    }
    this->output = new char[output_size]();
    this->shares = new sss_Share[output_size]();
  }
  ~CreateSharesWorker() {}

  void Execute() {
    sss_create_shares(shares, (uint8_t*) data, n, k, (uint8_t*) random_bytes);
    for (size_t idx = 0; idx < n; ++idx) {
      // Multiplication was already checked during constructor
      size_t offset = idx * sss_SHARE_SERIALIZED_LEN;
      sss_serialize_share((uint8_t*) &output[offset], &shares[idx]);
    }
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    // Copy the output to a node.js buffer
    v8::Local<v8::Array> array = v8::Array::New(isolate, n);
    for (size_t idx = 0; idx < n; ++idx) {
        size_t offset = idx * sss_SHARE_SERIALIZED_LEN;
        array->Set(idx, Nan::CopyBuffer((char*) &output[offset],
            sss_SHARE_SERIALIZED_LEN).ToLocalChecked());
    }
    v8::Local<v8::Value> argv[] = { array };

    // Call the provided callback
    callback->Call(1, argv);
  }

 private:
  uint8_t n, k;
  char data[sss_MLEN];
  char random_bytes[32];
  char *output;
  sss_Share *shares;
};


NAN_METHOD(CreateShares) {
  Nan::HandleScope scope;

  // Type check the argument `data`
  v8::Local<v8::Object> data = info[0]->ToObject();
  if (!node::Buffer::HasInstance(data)) {
    Nan::ThrowTypeError("`data` must be an instance of Buffer");
    return;
  }
  if (!info[1]->IsUint32()) {
    Nan::ThrowTypeError("`n` is not a valid integer");
    return;
  }
  uint32_t n = info[1]->Uint32Value();
  if (!info[2]->IsUint32()) {
    Nan::ThrowTypeError("`k` is not a valid integer");
    return;
  }
  uint32_t k = info[1]->Uint32Value();
  v8::Local<v8::Object> random_bytes = info[3]->ToObject();
  if (!node::Buffer::HasInstance(random_bytes)) {
    Nan::ThrowTypeError("`random_bytes` must be an instance of Buffer");
    return;
  }
  if (!info[4]->IsFunction()) {
    Nan::ThrowTypeError("`callback` must be a function");
    return;
  }
  Nan::Callback *callback = new Nan::Callback(info[4].As<v8::Function>());

  // Check if the buffers have the correct lengths
  if (node::Buffer::Length(data) != sss_MLEN) {
    Nan::ThrowRangeError("`data` buffer size must be exactly 64 bytes");
    return;
  };
  if (node::Buffer::Length(random_bytes) != 32) {
    Nan::ThrowRangeError("`random_bytes` buffer size must be exactly 32 bytes");
    return;
  };

  // Check if n and k are correct
  if (n < 1 || n > 255) {
    Nan::ThrowRangeError("`n` must be between 1 and 255");
    return;
  }
  if (k < 1 || k > n) {
    Nan::ThrowRangeError("`k` must be between 1 and n");
    return;
  }

  // Create worker
  CreateSharesWorker* worker = new CreateSharesWorker(
    node::Buffer::Data(data), n, k, node::Buffer::Data(random_bytes), callback);
  AsyncQueueWorker(worker);
}


void Initialize(v8::Local<v8::Object> exports, v8::Local<v8::Object> module) {
  Nan::HandleScope scope;
  Nan::SetMethod(exports, "createShares", CreateShares);
}


NODE_MODULE(shamirsecretsharing, Initialize)
