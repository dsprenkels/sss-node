#include <node.h>
#include <nan.h>
#include <memory>


extern "C" {
  #include "sss/sss.h"
}


struct SSSException : std::exception {
  SSSException(const char* msg) : msg(msg) {}
  const char* msg;
};

struct SSSTypeError : SSSException {
  SSSTypeError(const char* msg) : SSSException(msg) {}

};

struct SSSRangeError : SSSException {
  SSSRangeError(const char* msg) : SSSException(msg) {}
};


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
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    // Copy the output to a list of node.js buffers
    v8::Local<v8::Array> array = v8::Array::New(isolate, n);
    for (size_t idx = 0; idx < n; ++idx) {
      v8::Local<v8::Object> buf = Nan::CopyBuffer((char*) output[idx], sss_SHARE_LEN).ToLocalChecked();
      array->Set(context, idx, buf).ToChecked();
    }
    v8::Local<v8::Value> argv[] = { array };

    // Call the provided callback
    callback->Call(1, argv);
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
    for (unsigned idx = 0; idx < k; ++idx) {
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

    // Call the provided callback
    callback->Call(1, argv);
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
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    // Copy the output keyshares to a list of node.js buffers
    v8::Local<v8::Array> array = v8::Array::New(isolate, n);
    for (size_t idx = 0; idx < n; ++idx) {
      v8::Local<v8::Object> buf = Nan::CopyBuffer((char*) output[idx], sss_KEYSHARE_LEN).ToLocalChecked();
      array->Set(context, idx, buf);
    }
    v8::Local<v8::Value> argv[] = { array };

    // Call the provided callback
    callback->Call(1, argv);
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
    for (unsigned idx = 0; idx < k; ++idx) {
      memcpy(&this->input[idx], node::Buffer::Data(keyshares[idx]),
             sss_KEYSHARE_LEN);
    }
  }

  void Execute() {
    sss_combine_keyshares((uint8_t*) key, input.get(), k);
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;

    // Copy the key to a node.js buffer
    v8::Local<v8::Value> argv[] = { Nan::CopyBuffer(key, 32).ToLocalChecked() };

    // Call the provided callback
    callback->Call(1, argv);
  }

 private:
  uint8_t k;
  std::unique_ptr<sss_Keyshare[]> input;
  char key[32];
};


static void typeChk(bool cond, const char* msg) {
  if (!cond) {
    throw SSSTypeError(msg);
  }
}


static void rangeChk(bool cond, const char* msg) {
  if (!cond) {
    throw SSSRangeError(msg);
  }
}


std::pair<uint8_t, uint8_t>
unpackNK(v8::Local<v8::Value> n_val, v8::Local<v8::Value> k_val) {
  Nan::HandleScope scope;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (n_val->IsUndefined()) {
      throw SSSTypeError("`n` is not defined");
  }
  if (k_val->IsUndefined()) {
      throw SSSTypeError("`k` is not defined");
  }

  if (!n_val->IsUint32()) {
      throw SSSTypeError("`n` is not a valid integer");
  }
  if (!k_val->IsUint32()) {
      throw SSSTypeError("`k` is not a valid integer");
  }

  uint32_t n = n_val->Uint32Value(context).ToChecked();
  uint32_t k = k_val->Uint32Value(context).ToChecked();

  // Check if n and k are correct
  if (n < 1 || n > 255) {
    throw SSSRangeError("`n` must be between 1 and 255");
  }
  if (k < 1 || k > n) {
    throw SSSRangeError("`k` must be between 1 and n");
  }

  return std::make_pair(n, k);
}


char* unpackData(v8::Local<v8::Value> data_val) {
  Nan::HandleScope scope;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  typeChk(!data_val->IsUndefined(), "`data` is not defined");
  typeChk(data_val->IsObject(), "`data` is not an Object");
  v8::Local<v8::Object> data = data_val->ToObject(context).ToLocalChecked();
  typeChk(node::Buffer::HasInstance(data), "`data` must be a Buffer");
  // Check if the buffer has the correct length
  rangeChk(node::Buffer::Length(data) == sss_MLEN,
           "`data` buffer size must be exactly 64 bytes");
  return node::Buffer::Data(data);
}


char* unpackKey(v8::Local<v8::Value> key_val) {
  Nan::HandleScope scope;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  typeChk(!key_val->IsUndefined(), "`key` is not defined");
  typeChk(key_val->IsObject(), "`key` is not an Object");
  v8::Local<v8::Object> key = key_val->ToObject(context).ToLocalChecked();
  typeChk(node::Buffer::HasInstance(key), "`key` must be a Buffer");
  // Check if the buffer has the correct length
  rangeChk(node::Buffer::Length(key) == 32,
           "`key` buffer size must be exactly 64 bytes");
  return node::Buffer::Data(key);
}


Nan::Callback* unpackCallback(v8::Local<v8::Value> callback_val) {
  Nan::HandleScope scope;

  typeChk(!callback_val->IsUndefined(), "`callback` is not defined");
  typeChk(callback_val->IsFunction(), "`callback` must be a function");
  return new Nan::Callback(callback_val.As<v8::Function>());
}


NAN_METHOD(CreateShares) {
  Nan::HandleScope scope;

  try {
    char* data = unpackData(info[0]);
    std::pair<uint8_t, uint8_t> nk = unpackNK(info[1], info[2]);
    Nan::Callback* callback = unpackCallback(info[3]);

    // Create and start it
    CreateSharesWorker* worker = new CreateSharesWorker(
      data, std::get<0>(nk), std::get<1>(nk), callback);
    AsyncQueueWorker(worker);

  } catch (const SSSTypeError& e) {
    Nan::ThrowTypeError(e.msg);
  } catch (const SSSRangeError& e) {
    Nan::ThrowRangeError(e.msg);
  }
}


NAN_METHOD(CombineShares) {
  Nan::HandleScope scope;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  try {
    v8::Local<v8::Value> shares_val = info[0];

    // Type check `shares`
    typeChk(!shares_val->IsUndefined(), "`shares` is not defined");
    typeChk(shares_val->IsObject(), "`shares` is not an initialized Object");
    v8::Local<v8::Object> shares_obj = shares_val->ToObject(context).ToLocalChecked();
    typeChk(shares_val->IsArray(), "`shares` must be an array of buffers");
    v8::Local<v8::Array> shares_arr = shares_obj.As<v8::Array>();

    // Extract all the share buffers
    auto k = shares_arr->Length();
    std::unique_ptr<v8::Local<v8::Object>[]> shares(new v8::Local<v8::Object>[k]);
    for (unsigned idx = 0; idx < k; ++idx) {
      v8::Local<v8::Value> share_val = shares_arr->Get(context, idx).ToLocalChecked();
      v8::Local<v8::Object> share_obj = share_val->ToObject(context).ToLocalChecked();
      shares[idx] = share_obj;
    }

    // Check if all the elements in the array are buffers
    for (unsigned idx = 0; idx < k; ++idx) {
      typeChk(node::Buffer::HasInstance(shares[idx]),
              "array element is not a buffer");
    }

    // Check if all the elements in the array are of the correct length
    for (unsigned idx = 0; idx < k; ++idx) {
      rangeChk(node::Buffer::Length(shares[idx]) == sss_SHARE_LEN,
               "array buffer element is not of the correct length");
    }

    // Unpack the callback function
    Nan::Callback* callback = unpackCallback(info[1]);

    // Create worker
    CombineSharesWorker* worker = new CombineSharesWorker(shares, k, callback);
    AsyncQueueWorker(worker);
  } catch (const SSSTypeError& e) {
    Nan::ThrowTypeError(e.msg);
  } catch (const SSSRangeError& e) {
    Nan::ThrowRangeError(e.msg);
  }
}


NAN_METHOD(CreateKeyshares) {
  Nan::HandleScope scope;

  try {
    char* key = unpackKey(info[0]);
    std::pair<uint8_t, uint8_t> nk = unpackNK(info[1], info[2]);
    Nan::Callback *callback = unpackCallback(info[3]);

    // Create and start worker
    CreateKeysharesWorker* worker = new CreateKeysharesWorker(
      key, std::get<0>(nk), std::get<1>(nk), callback);
    AsyncQueueWorker(worker);
  } catch (const SSSTypeError& e) {
    Nan::ThrowTypeError(e.msg);
  } catch (const SSSRangeError& e) {
    Nan::ThrowRangeError(e.msg);
  }
}


NAN_METHOD(CombineKeyshares) {
  Nan::HandleScope scope;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  try {
    v8::Local<v8::Value> keyshares_val = info[0];

    // Type check `keyshares`
    typeChk(!keyshares_val->IsUndefined(), "`keyshares` is not defined");
    typeChk(keyshares_val->IsObject(), "`keyshares` is not an initialized Object");
    v8::Local<v8::Object> keyshares_obj = keyshares_val->ToObject(context).ToLocalChecked();
    typeChk(keyshares_val->IsArray(), "`keyshares` must be an array of buffers");
    v8::Local<v8::Array> keyshares_arr = keyshares_obj.As<v8::Array>();

    // Extract all the share buffers
    auto k = keyshares_arr->Length();
    std::unique_ptr<v8::Local<v8::Object>[]> keyshares(new v8::Local<v8::Object>[k]);
    for (unsigned idx = 0; idx < k; ++idx) {
      v8::Local<v8::Value> keyshare_val = keyshares_arr->Get(context, idx).ToLocalChecked();
      v8::Local<v8::Object> keyshare_obj = keyshare_val->ToObject(context).ToLocalChecked();
      keyshares[idx] = keyshare_obj;
    }

    // Check if all the elements in the array are buffers
    for (unsigned idx = 0; idx < k; ++idx) {
      typeChk(node::Buffer::HasInstance(keyshares[idx]),
              "array element is not a buffer");
    }

    // Check if all the elements in the array are of the correct length
    for (unsigned idx = 0; idx < k; ++idx) {
      rangeChk(node::Buffer::Length(keyshares[idx]) == sss_KEYSHARE_LEN,
               "array buffer element is not of the correct length");
    }

    // Unpack the callback function
    Nan::Callback* callback = unpackCallback(info[1]);

    // Create worker
    CombineKeysharesWorker* worker = new CombineKeysharesWorker(keyshares, k, callback);
    AsyncQueueWorker(worker);
  } catch (const SSSTypeError& e) {
    Nan::ThrowTypeError(e.msg);
  } catch (const SSSRangeError& e) {
    Nan::ThrowRangeError(e.msg);
  }
}


void Initialize(v8::Local<v8::Object> exports, v8::Local<v8::Object> module) {
  Nan::HandleScope scope;
  Nan::SetMethod(exports, "createShares", CreateShares);
  Nan::SetMethod(exports, "combineShares", CombineShares);
  Nan::SetMethod(exports, "createKeyshares", CreateKeyshares);
  Nan::SetMethod(exports, "combineKeyshares", CombineKeyshares);
}


NODE_MODULE(shamirsecretsharing, Initialize)
