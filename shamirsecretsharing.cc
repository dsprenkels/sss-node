#include <node.h>
#include <nan.h>
#include <memory>

#define TYPECHK(expr, msg) if (! expr) { \
  Nan::ThrowTypeError(msg); \
  return; \
}

extern "C" {
  #include "sss/sss.h"
  #include "sss/serialize.h"
}


class CreateSharesWorker : public Nan::AsyncWorker {
 public:
  CreateSharesWorker(char* data, uint8_t n, uint8_t k, char* random_bytes,
                     Nan::Callback *callback)
      : AsyncWorker(callback), n(n), k(k) {
    Nan::HandleScope scope;

    memcpy(this->data, data, sss_MLEN);
    memcpy(this->random_bytes, random_bytes, 32);
    size_t output_size = n * sss_SHARE_SERIALIZED_LEN;
    if (output_size / sss_SHARE_SERIALIZED_LEN != n) {
        // Overflow occurred! Is an unreachable state, because `n` is
        // between 0 and 256, and `sss_SHARE_SERIALIZED_LEN` is a small value.
        Nan::ThrowError("unreachable state");
    }
    this->output = std::unique_ptr<char[]>{ new char[output_size]() };
  }

  void Execute() {
    std::unique_ptr<sss_Share[]> shares(new sss_Share[n]());
    sss_create_shares(&shares[0], (uint8_t*) data, n, k,
                      (uint8_t*) random_bytes);
    for (size_t idx = 0; idx < n; ++idx) {
      // Multiplication was already checked during constructor
      size_t offset = idx * sss_SHARE_SERIALIZED_LEN;
      sss_serialize_share((uint8_t*) &output[offset], &shares[idx]);
    }
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    // Copy the output to a list of node.js buffers
    v8::Local<v8::Array> array = v8::Array::New(isolate, n);
    for (size_t idx = 0; idx < n; ++idx) {
        size_t offset = idx * sss_SHARE_SERIALIZED_LEN;
        array->Set(idx, Nan::CopyBuffer(&output[offset],
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
  std::unique_ptr<char[]> output;
};


class CombineSharesWorker : public Nan::AsyncWorker {
 public:
  CombineSharesWorker(v8::Local<v8::Object>* shares, uint8_t k,
                      Nan::Callback *callback)
      : AsyncWorker(callback), k(k) {
    Nan::HandleScope scope;

    size_t input_size = k * sss_SHARE_SERIALIZED_LEN;
    if (input_size / sss_SHARE_SERIALIZED_LEN != k) {
        // Overflow occurred! Is an unreachable state, because `k` is
        // between 0 and 256, and `sss_SHARE_SERIALIZED_LEN` is a small value.
        Nan::ThrowError("unreachable state");
    }
    this->input = std::unique_ptr<char[]>{ new char[input_size] };
    for (auto idx = 0; idx < k; ++idx) {
      memcpy(&this->input[idx * sss_SHARE_SERIALIZED_LEN],
             node::Buffer::Data(shares[idx]),
             sss_SHARE_SERIALIZED_LEN);
    }
  }

  void Execute() {
    std::unique_ptr<sss_Share[]> shares(new sss_Share[k]());
    for (auto idx = 0; idx < k; ++idx) {
      // Multiplication was already checked during constructor
      size_t offset = idx * sss_SHARE_SERIALIZED_LEN;
      sss_unserialize_share(&shares[idx], (uint8_t*) &input[offset]);
    }
    status = sss_combine_shares((uint8_t*) data, &shares[0], k);
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;

    if (status == 0) {
      // All went well, call the callback the restored buffer
      v8::Local<v8::Value> argv[] = {
        Nan::CopyBuffer(data, sss_MLEN).ToLocalChecked()
      };
      callback->Call(1, argv);
    } else {
      // Some kind of error occurred, reject the promise
      Nan::ThrowError("invalid or too fex shares provided");
    }
  }

 private:
  uint8_t k;
  std::unique_ptr<char[]> input;
  int status;
  char data[sss_MLEN];
};


NAN_METHOD(CreateShares) {
  Nan::HandleScope scope;

  v8::Local<v8::Value> data_val = info[0];
  v8::Local<v8::Value> n_val = info[1];
  v8::Local<v8::Value> k_val = info[2];
  v8::Local<v8::Value> random_bytes_val = info[3];
  v8::Local<v8::Value> callback_val = info[4];

  // Type check the arguments
  TYPECHK(!data_val->IsUndefined(), "`data` is not defined");
  TYPECHK(!n_val->IsUndefined(), "`n` is not defined");
  TYPECHK(!k_val->IsUndefined(), "`k` is not defined");
  TYPECHK(!random_bytes_val->IsUndefined(), "`random_bytes` is not defined");
  TYPECHK(!callback_val->IsUndefined(), "`callback` is not defined");

  TYPECHK(data_val->IsObject(), "`data` is not an Object");
  v8::Local<v8::Object> data = data_val->ToObject();
  TYPECHK(node::Buffer::HasInstance(data), "`data` must be a Buffer")
  TYPECHK(n_val->IsUint32(), "`n` is not a valid integer");
  uint32_t n = n_val->Uint32Value();
  TYPECHK(k_val->IsUint32(), "`k` is not a valid integer");
  uint32_t k = k_val->Uint32Value();
  TYPECHK(random_bytes_val->IsObject(), "`random_bytes` is not an Object");
  v8::Local<v8::Object> random_bytes = random_bytes_val->ToObject();
  TYPECHK(node::Buffer::HasInstance(random_bytes),
          "`random_bytes` must be a Buffer")
  TYPECHK(callback_val->IsFunction(), "`callback` must be a function");
  Nan::Callback *callback = new Nan::Callback(callback_val.As<v8::Function>());

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


NAN_METHOD(CombineShares) {
  Nan::HandleScope scope;

  v8::Local<v8::Value> shares_val = info[0];
  v8::Local<v8::Value> callback_val = info[1];

  // Type check the argument `shares` and `callback`
  TYPECHK(!shares_val->IsUndefined(), "`shares` is not defined");
  TYPECHK(!callback_val->IsUndefined(), "`callback` is not defined");

  TYPECHK(shares_val->IsObject(), "`data` is not an initialized Object")
  v8::Local<v8::Object> shares_obj = shares_val->ToObject();
  TYPECHK(shares_val->IsArray(), "`data` must be an array of buffers");
  v8::Local<v8::Array> shares_arr = shares_obj.As<v8::Array>();
  TYPECHK(callback_val->IsFunction(), "`callback` must be a function");
  Nan::Callback *callback = new Nan::Callback(callback_val.As<v8::Function>());

  // Extract all the share buffers
  auto k = shares_arr->Length();
  v8::Local<v8::Object> shares[k];
  for (auto idx = 0; idx < k; ++idx) {
    shares[idx] = shares_arr->Get(idx)->ToObject();
  }

  // Check if all the elements in the array are buffers
  for (auto idx = 0; idx < k; ++idx) {
    if (!node::Buffer::HasInstance(shares[idx])) {
      Nan::ThrowTypeError("array element is not a buffer");
      return;
    }
  }

  // Check if all the elements in the array are of the correct length
  for (auto idx = 0; idx < k; ++idx) {
    if (node::Buffer::Length(shares[idx]) != sss_SHARE_SERIALIZED_LEN) {
      Nan::ThrowTypeError("array buffer element is not of the correct length");
      return;
    }
  }

  // Create worker
  CombineSharesWorker* worker = new CombineSharesWorker(shares, k, callback);
  AsyncQueueWorker(worker);
}


void Initialize(v8::Local<v8::Object> exports, v8::Local<v8::Object> module) {
  Nan::HandleScope scope;
  Nan::SetMethod(exports, "createShares", CreateShares);
  Nan::SetMethod(exports, "combineShares", CombineShares);
}


NODE_MODULE(shamirsecretsharing, Initialize)
