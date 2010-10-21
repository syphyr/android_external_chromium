// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_var.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "third_party/ppapi/c/pp_var.h"
#include "third_party/ppapi/c/ppb_var.h"
#include "third_party/WebKit/WebKit/chromium/public/WebBindings.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_plugin_object.h"
#include "v8/include/v8.h"

using WebKit::WebBindings;

namespace pepper {

namespace {

const char kInvalidObjectException[] = "Error: Invalid object";
const char kInvalidPropertyException[] = "Error: Invalid property";
const char kInvalidValueException[] = "Error: Invalid value";
const char kUnableToGetPropertyException[] = "Error: Unable to get property";
const char kUnableToSetPropertyException[] = "Error: Unable to set property";
const char kUnableToRemovePropertyException[] =
    "Error: Unable to remove property";
const char kUnableToGetAllPropertiesException[] =
    "Error: Unable to get all properties";
const char kUnableToCallMethodException[] = "Error: Unable to call method";
const char kUnableToConstructException[] = "Error: Unable to construct";

// ---------------------------------------------------------------------------
// Utilities

// Converts the given PP_Var to an NPVariant, returning true on success.
// False means that the given variant is invalid. In this case, the result
// NPVariant will be set to a void one.
//
// The contents of the PP_Var will NOT be copied, so you need to ensure that
// the PP_Var remains valid while the resultant NPVariant is in use.
bool PPVarToNPVariantNoCopy(PP_Var var, NPVariant* result) {
  switch (var.type) {
    case PP_VARTYPE_VOID:
      VOID_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_NULL:
      NULL_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_BOOL:
      BOOLEAN_TO_NPVARIANT(var.value.as_bool, *result);
      break;
    case PP_VARTYPE_INT32:
      INT32_TO_NPVARIANT(var.value.as_int, *result);
      break;
    case PP_VARTYPE_DOUBLE:
      DOUBLE_TO_NPVARIANT(var.value.as_double, *result);
      break;
    case PP_VARTYPE_STRING: {
      scoped_refptr<StringVar> string(StringVar::FromPPVar(var));
      if (!string) {
        VOID_TO_NPVARIANT(*result);
        return false;
      }
      const std::string& value = string->value();
      STRINGN_TO_NPVARIANT(value.c_str(), value.size(), *result);
      break;
    }
    case PP_VARTYPE_OBJECT: {
      scoped_refptr<ObjectVar> object(ObjectVar::FromPPVar(var));
      if (!object) {
        VOID_TO_NPVARIANT(*result);
        return false;
      }
      OBJECT_TO_NPVARIANT(object->np_object(), *result);
      break;
    }
    default:
      VOID_TO_NPVARIANT(*result);
      return false;
  }
  return true;
}

// ObjectAccessorTryCatch ------------------------------------------------------

// Automatically sets up a TryCatch for accessing the object identified by the
// given PP_Var. The module from the object will be used for the exception
// strings generated by the TryCatch.
//
// This will automatically retrieve the ObjectVar from the object and throw
// an exception if it's invalid. At the end of construction, if there is no
// exception, you know that there is no previously set exception, that the
// object passed in is valid and ready to use (via the object() getter), and
// that the TryCatch's module() getter is also set up properly and ready to
// use.
class ObjectAccessorTryCatch : public TryCatch {
 public:
  ObjectAccessorTryCatch(PP_Var object, PP_Var* exception)
      : TryCatch(NULL, exception),
        object_(ObjectVar::FromPPVar(object)) {
    if (!object_) {
      // No object or an invalid object was given. This means we have no module
      // to associated with the exception text, so use the magic invalid object
      // exception.
      SetInvalidObjectException();
    } else {
      // When the object is valid, we have a valid module to associate
      set_module(object_->module());
    }
  }

  ObjectVar* object() { return object_.get(); }

 protected:
  scoped_refptr<ObjectVar> object_;

  DISALLOW_COPY_AND_ASSIGN(ObjectAccessorTryCatch);
};

// ObjectAccessiorWithIdentifierTryCatch ---------------------------------------

// Automatically sets up a TryCatch for accessing the identifier on the given
// object. This just extends ObjectAccessorTryCatch to additionally convert
// the given identifier to an NPIdentifier and validate it, throwing an
// exception if it's invalid.
//
// At the end of construction, if there is no exception, you know that there is
// no previously set exception, that the object passed in is valid and ready to
// use (via the object() getter), that the identifier is valid and ready to
// use (via the identifier() getter), and that the TryCatch's module() getter
// is also set up properly and ready to use.
class ObjectAccessorWithIdentifierTryCatch : public ObjectAccessorTryCatch {
 public:
  ObjectAccessorWithIdentifierTryCatch(PP_Var object,
                                       PP_Var identifier,
                                       PP_Var* exception)
      : ObjectAccessorTryCatch(object, exception),
        identifier_(0) {
    if (!has_exception()) {
      identifier_ = Var::PPVarToNPIdentifier(identifier);
      if (!identifier_)
        SetException(kInvalidPropertyException);
    }
  }

  NPIdentifier identifier() const { return identifier_; }

 private:
  NPIdentifier identifier_;

  DISALLOW_COPY_AND_ASSIGN(ObjectAccessorWithIdentifierTryCatch);
};

// PPB_Var methods -------------------------------------------------------------

PP_Var VarFromUtf8(PP_Module module_id, const char* data, uint32_t len) {
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return PP_MakeNull();
  return StringVar::StringToPPVar(module, data, len);
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  scoped_refptr<StringVar> str(StringVar::FromPPVar(var));
  if (!str) {
    *len = 0;
    return NULL;
  }
  *len = static_cast<uint32_t>(str->value().size());
  if (str->value().empty())
    return "";  // Don't return NULL on success.
  return str->value().data();
}

bool HasProperty(PP_Var var,
                 PP_Var name,
                 PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return false;
  return WebBindings::hasProperty(NULL, accessor.object()->np_object(),
                                  accessor.identifier());
}

bool HasMethod(PP_Var var,
               PP_Var name,
               PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return false;
  return WebBindings::hasMethod(NULL, accessor.object()->np_object(),
                                accessor.identifier());
}

PP_Var GetProperty(PP_Var var,
                   PP_Var name,
                   PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return PP_MakeVoid();

  NPVariant result;
  if (!WebBindings::getProperty(NULL, accessor.object()->np_object(),
                                accessor.identifier(), &result)) {
    // An exception may have been raised.
    accessor.SetException(kUnableToGetPropertyException);
    return PP_MakeVoid();
  }

  PP_Var ret = Var::NPVariantToPPVar(accessor.object()->module(), &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

void GetAllPropertyNames(PP_Var var,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  *properties = NULL;
  *property_count = 0;

  ObjectAccessorTryCatch accessor(var, exception);
  if (accessor.has_exception())
    return;

  NPIdentifier* identifiers = NULL;
  uint32_t count = 0;
  if (!WebBindings::enumerate(NULL, accessor.object()->np_object(),
                              &identifiers, &count)) {
    accessor.SetException(kUnableToGetAllPropertiesException);
    return;
  }

  if (count == 0)
    return;

  *property_count = count;
  *properties = static_cast<PP_Var*>(malloc(sizeof(PP_Var) * count));
  for (uint32_t i = 0; i < count; ++i) {
    (*properties)[i] = Var::NPIdentifierToPPVar(accessor.object()->module(),
                                                identifiers[i]);
  }
  free(identifiers);
}

void SetProperty(PP_Var var,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return;

  NPVariant variant;
  if (!PPVarToNPVariantNoCopy(value, &variant)) {
    accessor.SetException(kInvalidValueException);
    return;
  }
  if (!WebBindings::setProperty(NULL, accessor.object()->np_object(),
                                accessor.identifier(), &variant))
    accessor.SetException(kUnableToSetPropertyException);
}

void RemoveProperty(PP_Var var,
                    PP_Var name,
                    PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return;

  if (!WebBindings::removeProperty(NULL, accessor.object()->np_object(),
                                   accessor.identifier()))
    accessor.SetException(kUnableToRemovePropertyException);
}

PP_Var Call(PP_Var var,
            PP_Var method_name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  ObjectAccessorTryCatch accessor(var, exception);
  if (accessor.has_exception())
    return PP_MakeVoid();

  NPIdentifier identifier;
  if (method_name.type == PP_VARTYPE_VOID) {
    identifier = NULL;
  } else if (method_name.type == PP_VARTYPE_STRING) {
    // Specifically allow only string functions to be called.
    identifier = Var::PPVarToNPIdentifier(method_name);
    if (!identifier) {
      accessor.SetException(kInvalidPropertyException);
      return PP_MakeVoid();
    }
  } else {
    accessor.SetException(kInvalidPropertyException);
    return PP_MakeVoid();
  }

  scoped_array<NPVariant> args;
  if (argc) {
    args.reset(new NPVariant[argc]);
    for (uint32_t i = 0; i < argc; ++i) {
      if (!PPVarToNPVariantNoCopy(argv[i], &args[i])) {
        // This argument was invalid, throw an exception & give up.
        accessor.SetException(kInvalidValueException);
        return PP_MakeVoid();
      }
    }
  }

  bool ok;

  NPVariant result;
  if (identifier) {
    ok = WebBindings::invoke(NULL, accessor.object()->np_object(),
                             identifier, args.get(), argc, &result);
  } else {
    ok = WebBindings::invokeDefault(NULL, accessor.object()->np_object(),
                                    args.get(), argc, &result);
  }

  if (!ok) {
    // An exception may have been raised.
    accessor.SetException(kUnableToCallMethodException);
    return PP_MakeVoid();
  }

  PP_Var ret = Var::NPVariantToPPVar(accessor.module(), &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

PP_Var Construct(PP_Var var,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  ObjectAccessorTryCatch accessor(var, exception);
  if (accessor.has_exception())
    return PP_MakeVoid();

  scoped_array<NPVariant> args;
  if (argc) {
    args.reset(new NPVariant[argc]);
    for (uint32_t i = 0; i < argc; ++i) {
      if (!PPVarToNPVariantNoCopy(argv[i], &args[i])) {
        // This argument was invalid, throw an exception & give up.
        accessor.SetException(kInvalidValueException);
        return PP_MakeVoid();
      }
    }
  }

  NPVariant result;
  if (!WebBindings::construct(NULL, accessor.object()->np_object(),
                              args.get(), argc, &result)) {
    // An exception may have been raised.
    accessor.SetException(kUnableToConstructException);
    return PP_MakeVoid();
  }

  PP_Var ret = Var::NPVariantToPPVar(accessor.module(), &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

bool IsInstanceOf(PP_Var var,
                  const PPP_Class* ppp_class,
                  void** ppp_class_data) {
  scoped_refptr<ObjectVar> object(ObjectVar::FromPPVar(var));
  if (!object)
    return false;  // Not an object at all.

  return PluginObject::IsInstanceOf(object->np_object(),
                                    ppp_class, ppp_class_data);
}

PP_Var CreateObject(PP_Module module_id,
                    const PPP_Class* ppp_class,
                    void* ppp_class_data) {
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return PP_MakeNull();
  return PluginObject::Create(module, ppp_class, ppp_class_data);
}

const PPB_Var var_interface = {
  &Var::PluginAddRefPPVar,
  &Var::PluginReleasePPVar,
  &VarFromUtf8,
  &VarToUtf8,
  &HasProperty,
  &HasMethod,
  &GetProperty,
  &GetAllPropertyNames,
  &SetProperty,
  &RemoveProperty,
  &Call,
  &Construct,
  &IsInstanceOf,
  &CreateObject
};

}  // namespace

// Var -------------------------------------------------------------------------

Var::Var(PluginModule* module) : Resource(module) {
}

Var::~Var() {
}

// static
PP_Var Var::NPVariantToPPVar(PluginModule* module, const NPVariant* variant) {
  switch (variant->type) {
    case NPVariantType_Void:
      return PP_MakeVoid();
    case NPVariantType_Null:
      return PP_MakeNull();
    case NPVariantType_Bool:
      return PP_MakeBool(NPVARIANT_TO_BOOLEAN(*variant));
    case NPVariantType_Int32:
      return PP_MakeInt32(NPVARIANT_TO_INT32(*variant));
    case NPVariantType_Double:
      return PP_MakeDouble(NPVARIANT_TO_DOUBLE(*variant));
    case NPVariantType_String:
      return StringVar::StringToPPVar(
          module,
          NPVARIANT_TO_STRING(*variant).UTF8Characters,
          NPVARIANT_TO_STRING(*variant).UTF8Length);
    case NPVariantType_Object:
      return ObjectVar::NPObjectToPPVar(module, NPVARIANT_TO_OBJECT(*variant));
  }
  NOTREACHED();
  return PP_MakeVoid();
}

// static
NPIdentifier Var::PPVarToNPIdentifier(PP_Var var) {
  switch (var.type) {
    case PP_VARTYPE_STRING: {
      scoped_refptr<StringVar> string(StringVar::FromPPVar(var));
      if (!string)
        return NULL;
      return WebBindings::getStringIdentifier(string->value().c_str());
    }
    case PP_VARTYPE_INT32:
      return WebBindings::getIntIdentifier(var.value.as_int);
    default:
      return NULL;
  }
}

// static
PP_Var Var::NPIdentifierToPPVar(PluginModule* module, NPIdentifier id) {
  const NPUTF8* string_value = NULL;
  int32_t int_value = 0;
  bool is_string = false;
  WebBindings::extractIdentifierData(id, string_value, int_value, is_string);
  if (is_string)
    return StringVar::StringToPPVar(module, string_value);

  return PP_MakeInt32(int_value);
}

// static
void Var::PluginAddRefPPVar(PP_Var var) {
  if (var.type == PP_VARTYPE_STRING || var.type == PP_VARTYPE_OBJECT) {
    // TODO(brettw) consider checking that the ID is actually a var ID rather
    // than some random other resource ID.
    if (!ResourceTracker::Get()->AddRefResource(var.value.as_id))
      DLOG(WARNING) << "AddRefVar()ing a nonexistant string/object var.";
  }
}

// static
void Var::PluginReleasePPVar(PP_Var var) {
  if (var.type == PP_VARTYPE_STRING || var.type == PP_VARTYPE_OBJECT) {
    // TODO(brettw) consider checking that the ID is actually a var ID rather
    // than some random other resource ID.
    if (!ResourceTracker::Get()->UnrefResource(var.value.as_id))
      DLOG(WARNING) << "ReleaseVar()ing a nonexistant string/object var.";
  }
}

// static
const PPB_Var* Var::GetInterface() {
  return &var_interface;
}

// StringVar -------------------------------------------------------------------

StringVar::StringVar(PluginModule* module, const char* str, uint32 len)
    : Var(module),
      value_(str, len) {
}

StringVar::~StringVar() {
}

// static
PP_Var StringVar::StringToPPVar(PluginModule* module, const std::string& var) {
  return StringToPPVar(module, var.c_str(), var.size());
}

// static
PP_Var StringVar::StringToPPVar(PluginModule* module,
                                const char* data, uint32 len) {
  scoped_refptr<StringVar> str(new StringVar(module, data, len));
  if (!str || !IsStringUTF8(str->value()))
    return PP_MakeNull();

  PP_Var ret;
  ret.type = PP_VARTYPE_STRING;

  // The caller takes ownership now.
  ret.value.as_id = str->GetReference();
  return ret;
}

// static
scoped_refptr<StringVar> StringVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_STRING)
    return scoped_refptr<StringVar>(NULL);
  return Resource::GetAs<StringVar>(var.value.as_id);
}

// ObjectVar -------------------------------------------------------------

ObjectVar::ObjectVar(PluginModule* module, NPObject* np_object)
    : Var(module),
      np_object_(np_object) {
  WebBindings::retainObject(np_object_);
  module->AddNPObjectVar(this);
}

ObjectVar::~ObjectVar() {
  module()->RemoveNPObjectVar(this);
  WebBindings::releaseObject(np_object_);
}

// static
PP_Var ObjectVar::NPObjectToPPVar(PluginModule* module, NPObject* object) {
  scoped_refptr<ObjectVar> object_var(module->ObjectVarForNPObject(object));
  if (!object_var)  // No object for this module yet, make a new one.
    object_var = new ObjectVar(module, object);

  if (!object_var)
    return PP_MakeVoid();

  // Convert to a PP_Var, GetReference will AddRef for us.
  PP_Var result;
  result.type = PP_VARTYPE_OBJECT;
  result.value.as_id = object_var->GetReference();
  return result;
}

// static
scoped_refptr<ObjectVar> ObjectVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_OBJECT)
    return scoped_refptr<ObjectVar>(NULL);
  return Resource::GetAs<ObjectVar>(var.value.as_id);
}

// TryCatch --------------------------------------------------------------------

TryCatch::TryCatch(PluginModule* module, PP_Var* exception)
    : module_(module),
      has_exception_(exception && exception->type != PP_VARTYPE_VOID),
      exception_(exception) {
  WebBindings::pushExceptionHandler(&TryCatch::Catch, this);
}

TryCatch::~TryCatch() {
  WebBindings::popExceptionHandler();
}

void TryCatch::SetException(const char* message) {
  if (!module_) {
    // Don't have a module to make the string.
    SetInvalidObjectException();
    return;
  }

  if (!has_exception()) {
    has_exception_ = true;
    if (exception_)
      *exception_ = StringVar::StringToPPVar(module_, message, strlen(message));
  }
}

void TryCatch::SetInvalidObjectException() {
  if (!has_exception()) {
    has_exception_ = true;
    // TODO(brettw) bug 54504: Have a global singleton string that can hold
    // a generic error message.
    if (exception_)
      *exception_ = PP_MakeInt32(1);
  }
}

// static
void TryCatch::Catch(void* self, const char* message) {
  static_cast<TryCatch*>(self)->SetException(message);
}

}  // namespace pepper