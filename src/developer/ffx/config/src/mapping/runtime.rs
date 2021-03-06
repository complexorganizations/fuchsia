// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    crate::mapping::replace, crate::paths::get_runtime_base_path, lazy_static::lazy_static,
    regex::Regex, serde_json::Value,
};

pub(crate) fn runtime(value: Value) -> Option<Value> {
    lazy_static! {
        static ref REGEX: Regex = Regex::new(r"\$(RUNTIME)").unwrap();
    }

    replace(&*REGEX, get_runtime_base_path, value)
}

////////////////////////////////////////////////////////////////////////////////
// tests
#[cfg(test)]
mod test {
    use super::*;

    fn runtime_dir(default: &str) -> String {
        match get_runtime_base_path() {
            Ok(p) => p.to_str().map_or(default.to_string(), |s| s.to_string()),
            Err(_) => default.to_string(),
        }
    }

    #[test]
    fn test_mapper() {
        let value = runtime_dir("$RUNTIME");
        let test = Value::String("$RUNTIME".to_string());
        assert_eq!(runtime(test), Some(Value::String(value.to_string())));
    }

    #[test]
    fn test_mapper_multiple() {
        let value = runtime_dir("$RUNTIME");
        let test = Value::String("$RUNTIME/$RUNTIME".to_string());
        assert_eq!(runtime(test), Some(Value::String(format!("{}/{}", value, value))));
    }

    #[test]
    fn test_mapper_returns_pass_through() {
        let test = Value::String("$WHATEVER".to_string());
        assert_eq!(runtime(test), Some(Value::String("$WHATEVER".to_string())));
    }
}
