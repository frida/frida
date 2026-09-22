use alloc::ffi::CString;
use alloc::string::String;
use core::ffi::{CStr, c_char};

use crate::bindings::{
    GError, GumScriptApiValue, _GumScriptApiType_GUM_SCRIPT_API_BOOLEAN,
    _GumScriptApiType_GUM_SCRIPT_API_STRING, _GumScriptApiType_GUM_SCRIPT_API_UINT, gboolean,
    gpointer, gum_script_api_add_function, gum_script_api_new, gum_script_api_registry_add,
    gum_script_api_registry_obtain, gum_script_api_set_prelude, gum_script_api_unref,
};
use crate::linux::layout::{
    enumerate_constants_in, enumerate_fields_in, enumerate_parameters_in, field_offset_in,
    find_constant, name_of_type, resolve_struct, size_of_struct, size_of_type, types_are_described,
};

pub fn publish() {
    unsafe {
        let api = gum_script_api_new(c"Btf".as_ptr());

        gum_script_api_add_function(
            api,
            c"_findStruct".as_ptr(),
            c"s".as_ptr(),
            _GumScriptApiType_GUM_SCRIPT_API_UINT,
            Some(crate::signed_to_be_called_back(on_find_struct, 0)),
            core::ptr::null_mut(),
        );
        gum_script_api_add_function(
            api,
            c"_sizeOf".as_ptr(),
            c"u".as_ptr(),
            _GumScriptApiType_GUM_SCRIPT_API_UINT,
            Some(crate::signed_to_be_called_back(on_size_of, 0)),
            core::ptr::null_mut(),
        );
        gum_script_api_add_function(
            api,
            c"_findOffsetOf".as_ptr(),
            c"us".as_ptr(),
            _GumScriptApiType_GUM_SCRIPT_API_UINT,
            Some(crate::signed_to_be_called_back(on_find_offset, 0)),
            core::ptr::null_mut(),
        );
        gum_script_api_add_function(
            api,
            c"_fieldsOf".as_ptr(),
            c"u".as_ptr(),
            _GumScriptApiType_GUM_SCRIPT_API_STRING,
            Some(crate::signed_to_be_called_back(on_fields_of, 0)),
            core::ptr::null_mut(),
        );

        gum_script_api_add_function(
            api,
            c"_findConstant".as_ptr(),
            c"s".as_ptr(),
            _GumScriptApiType_GUM_SCRIPT_API_STRING,
            Some(crate::signed_to_be_called_back(on_find_constant, 0)),
            core::ptr::null_mut(),
        );
        gum_script_api_add_function(
            api,
            c"_constantsOf".as_ptr(),
            c"s".as_ptr(),
            _GumScriptApiType_GUM_SCRIPT_API_STRING,
            Some(crate::signed_to_be_called_back(on_constants_of, 0)),
            core::ptr::null_mut(),
        );
        gum_script_api_add_function(
            api,
            c"_signatureOf".as_ptr(),
            c"s".as_ptr(),
            _GumScriptApiType_GUM_SCRIPT_API_STRING,
            Some(crate::signed_to_be_called_back(on_signature_of, 0)),
            core::ptr::null_mut(),
        );
        gum_script_api_add_function(
            api,
            c"_isAvailable".as_ptr(),
            c"".as_ptr(),
            _GumScriptApiType_GUM_SCRIPT_API_BOOLEAN,
            Some(crate::signed_to_be_called_back(on_is_available, 0)),
            core::ptr::null_mut(),
        );

        gum_script_api_set_prelude(api, PRELUDE.as_ptr());

        gum_script_api_registry_add(gum_script_api_registry_obtain(), api);
        gum_script_api_unref(api);
    }
}

unsafe extern "C" fn on_find_struct(
    args: *const GumScriptApiValue,
    retval: *mut GumScriptApiValue,
    _user_data: gpointer,
    _error: *mut *mut GError,
) -> gboolean {
    let container = unsafe { text_of(args, 0) };

    let handle = resolve_struct(container).map_or(NOTHING, |handle| (handle + 1) as u32);

    unsafe { (*retval).__bindgen_anon_1.u = handle };

    1
}

unsafe extern "C" fn on_size_of(
    args: *const GumScriptApiValue,
    retval: *mut GumScriptApiValue,
    _user_data: gpointer,
    _error: *mut *mut GError,
) -> gboolean {
    let handle = unsafe { handle_of(args, 0) };

    unsafe { (*retval).__bindgen_anon_1.u = size_of_struct(handle) as u32 };

    1
}

unsafe extern "C" fn on_find_offset(
    args: *const GumScriptApiValue,
    retval: *mut GumScriptApiValue,
    _user_data: gpointer,
    _error: *mut *mut GError,
) -> gboolean {
    let handle = unsafe { handle_of(args, 0) };
    let field = unsafe { text_of(args, 1) };

    let offset = field_offset_in(handle, field).map_or(NOWHERE, |offset| offset as u32);

    unsafe { (*retval).__bindgen_anon_1.u = offset };

    1
}

unsafe extern "C" fn on_is_available(
    _args: *const GumScriptApiValue,
    retval: *mut GumScriptApiValue,
    _user_data: gpointer,
    _error: *mut *mut GError,
) -> gboolean {
    unsafe { (*retval).__bindgen_anon_1.b = types_are_described() as gboolean };

    1
}

unsafe extern "C" fn on_fields_of(
    args: *const GumScriptApiValue,
    retval: *mut GumScriptApiValue,
    _user_data: gpointer,
    _error: *mut *mut GError,
) -> gboolean {
    let handle = unsafe { handle_of(args, 0) };

    let mut described = String::from("{");
    let mut separator = "";
    enumerate_fields_in(handle, &mut |name, field| {
        described.push_str(&alloc::format!(
            "{separator}\"{name}\":{{\"offset\":{},\"size\":{},\"type\":\"{}\"",
            field.offset,
            size_of_type(field.id),
            name_of_type(field.id)
        ));
        if field.bit_size != 0 {
            described.push_str(&alloc::format!(
                ",\"bitOffset\":{},\"bitSize\":{}",
                field.bit_offset, field.bit_size
            ));
        }
        described.push('}');
        separator = ",";
        true
    });
    described.push('}');

    unsafe { answer_with(retval, described) };

    1
}

unsafe extern "C" fn on_find_constant(
    args: *const GumScriptApiValue,
    retval: *mut GumScriptApiValue,
    _user_data: gpointer,
    _error: *mut *mut GError,
) -> gboolean {
    let name = unsafe { text_of(args, 0) };

    let described = find_constant(name).map_or_else(String::new, |value| alloc::format!("{value}"));

    unsafe { answer_with(retval, described) };

    1
}

unsafe extern "C" fn on_constants_of(
    args: *const GumScriptApiValue,
    retval: *mut GumScriptApiValue,
    _user_data: gpointer,
    _error: *mut *mut GError,
) -> gboolean {
    let container = unsafe { text_of(args, 0) };

    let mut described = String::from("{");
    let mut separator = "";
    let found = enumerate_constants_in(container, &mut |name, value| {
        described.push_str(&alloc::format!("{separator}\"{name}\":{value}"));
        separator = ",";
    });
    described.push('}');

    unsafe { answer_with(retval, if found { described } else { String::new() }) };

    1
}

unsafe extern "C" fn on_signature_of(
    args: *const GumScriptApiValue,
    retval: *mut GumScriptApiValue,
    _user_data: gpointer,
    _error: *mut *mut GError,
) -> gboolean {
    let name = unsafe { text_of(args, 0) };

    let mut parameters = String::from("[");
    let mut separator = "";
    let returned = enumerate_parameters_in(name, &mut |name, kind| {
        parameters.push_str(&alloc::format!(
            "{separator}{{\"name\":\"{name}\",\"type\":\"{kind}\"}}"
        ));
        separator = ",";
    });
    parameters.push(']');

    let described = match returned {
        Some(returned) => {
            alloc::format!("{{\"returns\":\"{returned}\",\"parameters\":{parameters}}}")
        }
        None => String::new(),
    };

    unsafe { answer_with(retval, described) };

    1
}

unsafe fn answer_with(retval: *mut GumScriptApiValue, described: String) {
    unsafe {
        let holder = (&raw mut LAST_ANSWER).as_mut().unwrap();
        *holder = Some(CString::new(described).unwrap());
        (*retval).__bindgen_anon_1.s = holder.as_ref().unwrap().as_ptr();
    }
}

static mut LAST_ANSWER: Option<CString> = None;

const NOTHING: u32 = 0;
const NOWHERE: u32 = u32::MAX;

unsafe fn handle_of(args: *const GumScriptApiValue, index: usize) -> usize {
    unsafe { (*args.add(index)).__bindgen_anon_1.u as usize - 1 }
}

unsafe fn text_of(args: *const GumScriptApiValue, index: usize) -> &'static str {
    unsafe {
        CStr::from_ptr((*args.add(index)).__bindgen_anon_1.s as *const c_char)
            .to_str()
            .unwrap_or("")
    }
}

const PRELUDE: &CStr = c"
(() => {
    const { _findStruct, _sizeOf, _findOffsetOf, _fieldsOf, _findConstant, _constantsOf,
        _signatureOf, _isAvailable } = Btf;

    const NOWHERE = 0xffffffff;

    class BtfStruct {
        constructor(name, handle) {
            this.name = name;
            this.handle = handle;
            this.size = _sizeOf(handle);
            this.known = new Map();
        }

        getOffsetOf(field) {
            const offset = this.findOffsetOf(field);
            if (offset === null)
                throw new Error(`${this.name}: unable to find field '${field}'`);
            return offset;
        }

        findOffsetOf(field) {
            let offset = this.known.get(field);
            if (offset === undefined) {
                offset = _findOffsetOf(this.handle, field);
                if (offset === NOWHERE)
                    return null;
                this.known.set(field, offset);
            }
            return offset;
        }

        get fields() {
            const fields = JSON.parse(_fieldsOf(this.handle));
            Object.defineProperty(this, 'fields', { value: fields });
            return fields;
        }

        toString() {
            return `BtfStruct(${this.name}, size: ${this.size})`;
        }
    }

    const resolved = new Map();
    const functions = new Map();

    Btf.available = _isAvailable();

    Btf.getStruct = name => {
        const described = Btf.findStruct(name);
        if (described === null)
            throw new Error(`unable to find struct '${name}'`);
        return described;
    };

    Btf.findStruct = name => {
        let described = resolved.get(name);
        if (described === undefined) {
            const handle = _findStruct(name);
            if (handle === 0)
                return null;
            described = new BtfStruct(name, handle);
            resolved.set(name, described);
        }
        return described;
    };

    Btf.getConstant = name => {
        const value = Btf.findConstant(name);
        if (value === null)
            throw new Error(`unable to find constant '${name}'`);
        return value;
    };

    Btf.findConstant = name => {
        const value = _findConstant(name);
        if (value === '')
            return null;
        return numberFrom(value);
    };

    Btf.getEnum = name => {
        const constants = Btf.findEnum(name);
        if (constants === null)
            throw new Error(`unable to find enum '${name}'`);
        return constants;
    };

    Btf.findEnum = name => {
        const constants = _constantsOf(name);
        if (constants === '')
            return null;
        return JSON.parse(constants);
    };

    Btf.getFunction = name => {
        const signature = Btf.findFunction(name);
        if (signature === null)
            throw new Error(`unable to find function '${name}'`);
        return signature;
    };

    Btf.findFunction = name => {
        let signature = functions.get(name);
        if (signature === undefined) {
            const described = _signatureOf(name);
            if (described === '')
                return null;
            signature = JSON.parse(described);
            functions.set(name, signature);
        }
        return signature;
    };

    function numberFrom(text) {
        const value = parseInt(text);
        return Number.isSafeInteger(value) ? value : int64(text);
    }
})();
";
