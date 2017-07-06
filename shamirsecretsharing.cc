#include <node.h>
#include <nan.h>
#include <memory>

#define TYPECHK(expr, msg) if (! expr) { \
  Nan::ThrowTypeError(msg); \
  return; \
}

extern "C" {
  #include "sss/sss.h"
}


class CreateSharesWorker : public Nan::AsyncWorker {
 public:
  CreateSharesWorker(char* data, uint8_t n, uint8_t k, Nan::Callback *callback)
      : AsyncWorker(callback), n(n), k(k) {
    Nan::HandleScope scope;

    memcpy(this->data, data, sss_MLEN);
  }

  void Execute() {
    this->output = std::unique_ptr<sss_Share[]>{ new sss_Share[n]() };
    sss_create_shares(output.get(), (uint8_t*) data, n, k);
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    // Copy the output to a list of node.js buffers
    v8::Local<v8::Array> array = v8::Array::New(isolate, n);
    for (size_t idx = 0; idx < n; ++idx) {
      array->Set(idx, Nan::CopyBuffer((char*) output[idx],
        sss_SHARE_LEN).ToLocalChecked());
    }
    v8::Local<v8::Value> argv[] = { array };

    // Call the provided callback
    Nan::Call(**callback, Nan::GetCurrentContext()->Global(), 1, argv);
  }

 private:
  uint8_t n, k;
  char data[sss_MLEN];
  std::unique_ptr<sss_Share[]> output;
};


class CombineSharesWorker : public Nan::AsyncWorker {
 public:
  CombineSharesWorker(std::unique_ptr<v8::Local<v8::Object>[]> &shares,
                      uint8_t k, Nan::Callback *callback)
      : AsyncWorker(callback), k(k) {
    Nan::HandleScope scope;

    this->input = std::unique_ptr<sss_Share[]>{ new sss_Share[k] };
    for (auto idx = 0; idx < k; ++idx) {
      memcpy(&this->input[idx], node::Buffer::Data(shares[idx]), sss_SHARE_LEN);
    }
  }

  void Execute() {
    status = sss_combine_shares((uint8_t*) data, input.get(), k);
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;

    v8::Local<v8::Value> argv[1];
    if (status == 0) {
      // All went well, call the callback the restored buffer
      argv[0] = Nan::CopyBuffer(data, sss_MLEN).ToLocalChecked();
    } else {
      // Some kind of error occurred, return null
      argv[0] = Nan::Null();
    }
    Nan::Call(**callback, Nan::GetCurrentContext()->Global(), 1, argv);
  }

 private:
  uint8_t k;
  std::unique_ptr<sss_Share[]> input;
  int status;
  char data[sss_MLEN];
};


class CreateKeysharesWorker : public Nan::AsyncWorker {
 public:
  CreateKeysharesWorker(char* key, uint8_t n, uint8_t k, Nan::Callback *callback)
      : AsyncWorker(callback), n(n), k(k) {
    Nan::HandleScope scope;

    memcpy(this->key, key, 32);
  }

  void Execute() {
    this->output = std::unique_ptr<sss_Keyshare[]>{ new sss_Keyshare[n]() };
    sss_create_keyshares(output.get(), (uint8_t*) key, n, k);
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    // Copy the output keyshares to a list of node.js buffers
    v8::Local<v8::Array> array = v8::Array::New(isolate, n);
    for (size_t idx = 0; idx < n; ++idx) {
      array->Set(idx, Nan::CopyBuffer((char*) output[idx],
                 sss_KEYSHARE_LEN).ToLocalChecked());
    }
    v8::Local<v8::Value> argv[] = { array };

    // Call the provided callback
    Nan::Call(**callback, Nan::GetCurrentContext()->Global(), 1, argv);
  }

 private:
  uint8_t n, k;
  char key[32];
  std::unique_ptr<sss_Keyshare[]> output;
};


class CombineKeysharesWorker : public Nan::AsyncWorker {
 public:
  CombineKeysharesWorker(std::unique_ptr<v8::Local<v8::Object>[]> &keyshares,
                         uint8_t k, Nan::Callback *callback)
      : AsyncWorker(callback), k(k) {
    Nan::HandleScope scope;

    this->input = std::unique_ptr<sss_Keyshare[]>{ new sss_Keyshare[k] };
    for (auto idx = 0; idx < k; ++idx) {
      memcpy(&this->input[idx], node::Buffer::Data(keyshares[idx]),
             sss_KEYSHARE_LEN);
    }
  }

  void Execute() {
    sss_combine_keyshares((uint8_t*) key, input.get(), k);
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;

    v8::Local<v8::Value> argv[] = { Nan::CopyBuffer(key, 32).ToLocalChecked() };
    Nan::Call(**callback, Nan::GetCurrentContext()->Global(), 1, argv);
  }

 private:
  uint8_t k;
  std::unique_ptr<sss_Keyshare[]> input;
  char key[32];
};


NAN_METHOD(CreateShares) {
  Nan::HandleScope scope;

  v8::Local<v8::Value> data_val = info[0];
  v8::Local<v8::Value> n_val = info[1];
  v8::Local<v8::Value> k_val = info[2];
  v8::Local<v8::Value> callback_val = info[3];

  // Type check the arguments
  TYPECHK(!data_val->IsUndefined(), "`data` is not defined");
  TYPECHK(!n_val->IsUndefined(), "`n` is not defined");
  TYPECHK(!k_val->IsUndefined(), "`k` is not defined");
  TYPECHK(!callback_val->IsUndefined(), "`callback` is not defined");

  TYPECHK(data_val->IsObject(), "`data` is not an Object");
  v8::Local<v8::Object> data = data_val->ToObject();
  TYPECHK(node::Buffer::HasInstance(data), "`data` must be a Buffer")
  TYPECHK(n_val->IsUint32(), "`n` is not a valid integer");
  uint32_t n = n_val->Uint32Value();
  TYPECHK(k_val->IsUint32(), "`k` is not a valid integer");
  uint32_t k = k_val->Uint32Value();
  TYPECHK(callback_val->IsFunction(), "`callback` must be a function");
  Nan::Callback *callback = new Nan::Callback(callback_val.As<v8::Function>());

  // Check if the buffers have the correct lengths
  if (node::Buffer::Length(data) != sss_MLEN) {
    Nan::ThrowRangeError("`data` buffer size must be exactly 64 bytes");
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
    node::Buffer::Data(data), n, k, callback);
  AsyncQueueWorker(worker);
}


NAN_METHOD(CombineShares) {
  Nan::HandleScope scope;

  v8::Local<v8::Value> shares_val = info[0];
  v8::Local<v8::Value> callback_val = info[1];

  // Type check the argument `shares` and `callback`
  TYPECHK(!shares_val->IsUndefined(), "`shares` is not defined");
  TYPECHK(!callback_val->IsUndefined(), "`callback` is not defined");

  TYPECHK(shares_val->IsObject(), "`shares` is not an initialized Object")
  v8::Local<v8::Object> shares_obj = shares_val->ToObject();
  TYPECHK(shares_val->IsArray(), "`shares` must be an array of buffers");
  v8::Local<v8::Array> shares_arr = shares_obj.As<v8::Array>();
  TYPECHK(callback_val->IsFunction(), "`callback` must be a function");
  Nan::Callback *callback = new Nan::Callback(callback_val.As<v8::Function>());

  // Extract all the share buffers
  auto k = shares_arr->Length();
  std::unique_ptr<v8::Local<v8::Object>[]> shares(new v8::Local<v8::Object>[k]);
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
    if (node::Buffer::Length(shares[idx]) != sss_SHARE_LEN) {
      Nan::ThrowTypeError("array buffer element is not of the correct length");
      return;
    }
  }

  // Create worker
  CombineSharesWorker* worker = new CombineSharesWorker(shares, k, callback);
  AsyncQueueWorker(worker);
}


NAN_METHOD(CreateKeyshares) {
  Nan::HandleScope scope;

  v8::Local<v8::Value> key_val = info[0];
  v8::Local<v8::Value> n_val = info[1];
  v8::Local<v8::Value> k_val = info[2];
  v8::Local<v8::Value> callback_val = info[3];

  // Type check the arguments
  TYPECHK(!key_val->IsUndefined(), "`key` is not defined");
  TYPECHK(!n_val->IsUndefined(), "`n` is not defined");
  TYPECHK(!k_val->IsUndefined(), "`k` is not defined");
  TYPECHK(!callback_val->IsUndefined(), "`callback` is not defined");

  TYPECHK(key_val->IsObject(), "`key` is not an Object");
  v8::Local<v8::Object> key = key_val->ToObject();
  TYPECHK(node::Buffer::HasInstance(key), "`key` must be a Buffer")
  TYPECHK(n_val->IsUint32(), "`n` is not a valid integer");
  uint32_t n = n_val->Uint32Value();
  TYPECHK(k_val->IsUint32(), "`k` is not a valid integer");
  uint32_t k = k_val->Uint32Value();
  TYPECHK(callback_val->IsFunction(), "`callback` must be a function");
  Nan::Callback *callback = new Nan::Callback(callback_val.As<v8::Function>());

  // Check if the buffers have the correct lengths
  if (node::Buffer::Length(key) != 32) {
    Nan::ThrowRangeError("`key` buffer size must be exactly 32 bytes");
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
  CreateKeysharesWorker* worker = new CreateKeysharesWorker(
    node::Buffer::Data(key), n, k, callback);
  AsyncQueueWorker(worker);
}


NAN_METHOD(CombineKeyshares) {
  Nan::HandleScope scope;

  v8::Local<v8::Value> keyshares_val = info[0];
  v8::Local<v8::Value> callback_val = info[1];

  // Type check the argument `keyshares` and `callback`
  TYPECHK(!keyshares_val->IsUndefined(), "`keyshares` is not defined");
  TYPECHK(!callback_val->IsUndefined(), "`callback` is not defined");

  TYPECHK(keyshares_val->IsObject(), "`keyshares` is not an initialized Object")
  v8::Local<v8::Object> keyshares_obj = keyshares_val->ToObject();
  TYPECHK(keyshares_val->IsArray(), "`keyshares` must be an array of buffers");
  v8::Local<v8::Array> keyshares_arr = keyshares_obj.As<v8::Array>();
  TYPECHK(callback_val->IsFunction(), "`callback` must be a function");
  Nan::Callback *callback = new Nan::Callback(callback_val.As<v8::Function>());

  // Extract all the share buffers
  auto k = keyshares_arr->Length();
  std::unique_ptr<v8::Local<v8::Object>[]> keyshares(new v8::Local<v8::Object>[k]);
  for (auto idx = 0; idx < k; ++idx) {
    keyshares[idx] = keyshares_arr->Get(idx)->ToObject();
  }

  // Check if all the elements in the array are buffers
  for (auto idx = 0; idx < k; ++idx) {
    if (!node::Buffer::HasInstance(keyshares[idx])) {
      Nan::ThrowTypeError("array element is not a buffer");
      return;
    }
  }

  // Check if all the elements in the array are of the correct length
  for (auto idx = 0; idx < k; ++idx) {
    if (node::Buffer::Length(keyshares[idx]) != sss_KEYSHARE_LEN) {
      Nan::ThrowTypeError("array buffer element is not of the correct length");
      return;
    }
  }

  // Create worker
  CombineKeysharesWorker* worker = new CombineKeysharesWorker(keyshares, k, callback);
  AsyncQueueWorker(worker);
}


void Initialize(v8::Local<v8::Object> exports, v8::Local<v8::Object> module) {
  Nan::HandleScope scope;
  Nan::SetMethod(exports, "createShares", CreateShares);
  Nan::SetMethod(exports, "combineShares", CombineShares);
  Nan::SetMethod(exports, "createKeyshares", CreateKeyshares);
  Nan::SetMethod(exports, "combineKeyshares", CombineKeyshares);
}


NODE_MODULE(shamirsecretsharing, Initialize)
