// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"

namespace base {

TEST(JSONReaderTest, Reading) {
  // some whitespace checking
  scoped_ptr<Value> root;
  root.reset(JSONReader().JsonToValue("   null   ", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_NULL));

  // Invalid JSON string
  root.reset(JSONReader().JsonToValue("nu", false, false));
  ASSERT_FALSE(root.get());

  // Simple bool
  root.reset(JSONReader().JsonToValue("true  ", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_BOOLEAN));

  // Embedded comment
  root.reset(JSONReader().JsonToValue("/* comment */null", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_NULL));
  root.reset(JSONReader().JsonToValue("40 /* comment */", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_INTEGER));
  root.reset(JSONReader().JsonToValue("true // comment", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_BOOLEAN));
  root.reset(JSONReader().JsonToValue("/* comment */\"sample string\"",
                                      false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_STRING));
  std::string value;
  ASSERT_TRUE(root->GetAsString(&value));
  ASSERT_EQ("sample string", value);

  // Test number formats
  root.reset(JSONReader().JsonToValue("43", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_INTEGER));
  int int_val = 0;
  ASSERT_TRUE(root->GetAsInteger(&int_val));
  ASSERT_EQ(43, int_val);

  // According to RFC4627, oct, hex, and leading zeros are invalid JSON.
  root.reset(JSONReader().JsonToValue("043", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("0x43", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("00", false, false));
  ASSERT_FALSE(root.get());

  // Test 0 (which needs to be special cased because of the leading zero
  // clause).
  root.reset(JSONReader().JsonToValue("0", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_INTEGER));
  int_val = 1;
  ASSERT_TRUE(root->GetAsInteger(&int_val));
  ASSERT_EQ(0, int_val);

  // Numbers that overflow ints should succeed, being internally promoted to
  // storage as doubles
  root.reset(JSONReader().JsonToValue("2147483648", false, false));
  ASSERT_TRUE(root.get());
  double double_val;
  ASSERT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  ASSERT_TRUE(root->GetAsDouble(&double_val));
  ASSERT_DOUBLE_EQ(2147483648.0, double_val);
  root.reset(JSONReader().JsonToValue("-2147483649", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  ASSERT_TRUE(root->GetAsDouble(&double_val));
  ASSERT_DOUBLE_EQ(-2147483649.0, double_val);

  // Parse a double
  root.reset(JSONReader().JsonToValue("43.1", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  ASSERT_TRUE(root->GetAsDouble(&double_val));
  ASSERT_DOUBLE_EQ(43.1, double_val);

  root.reset(JSONReader().JsonToValue("4.3e-1", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  ASSERT_TRUE(root->GetAsDouble(&double_val));
  ASSERT_DOUBLE_EQ(.43, double_val);

  root.reset(JSONReader().JsonToValue("2.1e0", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  ASSERT_TRUE(root->GetAsDouble(&double_val));
  ASSERT_DOUBLE_EQ(2.1, double_val);

  root.reset(JSONReader().JsonToValue("2.1e+0001", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  ASSERT_TRUE(root->GetAsDouble(&double_val));
  ASSERT_DOUBLE_EQ(21.0, double_val);

  root.reset(JSONReader().JsonToValue("0.01", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  ASSERT_TRUE(root->GetAsDouble(&double_val));
  ASSERT_DOUBLE_EQ(0.01, double_val);

  root.reset(JSONReader().JsonToValue("1.00", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  ASSERT_TRUE(root->GetAsDouble(&double_val));
  ASSERT_DOUBLE_EQ(1.0, double_val);

  // Fractional parts must have a digit before and after the decimal point.
  root.reset(JSONReader().JsonToValue("1.", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue(".1", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("1.e10", false, false));
  ASSERT_FALSE(root.get());

  // Exponent must have a digit following the 'e'.
  root.reset(JSONReader().JsonToValue("1e", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("1E", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("1e1.", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("1e1.0", false, false));
  ASSERT_FALSE(root.get());

  // INF/-INF/NaN are not valid
  root.reset(JSONReader().JsonToValue("1e1000", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("-1e1000", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("NaN", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("nan", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("inf", false, false));
  ASSERT_FALSE(root.get());

  // Invalid number formats
  root.reset(JSONReader().JsonToValue("4.3.1", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("4e3.1", false, false));
  ASSERT_FALSE(root.get());

  // Test string parser
  root.reset(JSONReader().JsonToValue("\"hello world\"", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_STRING));
  std::string str_val;
  ASSERT_TRUE(root->GetAsString(&str_val));
  ASSERT_EQ("hello world", str_val);

  // Empty string
  root.reset(JSONReader().JsonToValue("\"\"", false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  ASSERT_TRUE(root->GetAsString(&str_val));
  ASSERT_EQ("", str_val);

  // Test basic string escapes
  root.reset(JSONReader().JsonToValue("\" \\\"\\\\\\/\\b\\f\\n\\r\\t\\v\"",
                                      false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  ASSERT_TRUE(root->GetAsString(&str_val));
  ASSERT_EQ(" \"\\/\b\f\n\r\t\v", str_val);

  // Test hex and unicode escapes including the null character.
  root.reset(JSONReader().JsonToValue("\"\\x41\\x00\\u1234\"", false,
                                      false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  ASSERT_TRUE(root->GetAsString(&str_val));
  ASSERT_EQ(std::wstring(L"A\0\x1234", 3), UTF8ToWide(str_val));

  // Test invalid strings
  root.reset(JSONReader().JsonToValue("\"no closing quote", false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("\"\\z invalid escape char\"", false,
                                      false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("\"\\xAQ invalid hex code\"", false,
                                      false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("not enough hex chars\\x1\"", false,
                                      false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("\"not enough escape chars\\u123\"",
                                      false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("\"extra backslash at end of input\\\"",
                                      false, false));
  ASSERT_FALSE(root.get());

  // Basic array
  root.reset(JSONReader::Read("[true, false, null]", false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_LIST));
  ListValue* list = static_cast<ListValue*>(root.get());
  ASSERT_EQ(3U, list->GetSize());

  // Test with trailing comma.  Should be parsed the same as above.
  scoped_ptr<Value> root2;
  root2.reset(JSONReader::Read("[true, false, null, ]", true));
  EXPECT_TRUE(root->Equals(root2.get()));

  // Empty array
  root.reset(JSONReader::Read("[]", false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root.get());
  ASSERT_EQ(0U, list->GetSize());

  // Nested arrays
  root.reset(JSONReader::Read("[[true], [], [false, [], [null]], null]",
                              false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root.get());
  ASSERT_EQ(4U, list->GetSize());

  // Lots of trailing commas.
  root2.reset(JSONReader::Read("[[true], [], [false, [], [null, ]  , ], null,]",
                               true));
  EXPECT_TRUE(root->Equals(root2.get()));

  // Invalid, missing close brace.
  root.reset(JSONReader::Read("[[true], [], [false, [], [null]], null", false));
  ASSERT_FALSE(root.get());

  // Invalid, too many commas
  root.reset(JSONReader::Read("[true,, null]", false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader::Read("[true,, null]", true));
  ASSERT_FALSE(root.get());

  // Invalid, no commas
  root.reset(JSONReader::Read("[true null]", false));
  ASSERT_FALSE(root.get());

  // Invalid, trailing comma
  root.reset(JSONReader::Read("[true,]", false));
  ASSERT_FALSE(root.get());

  // Valid if we set |allow_trailing_comma| to true.
  root.reset(JSONReader::Read("[true,]", true));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root.get());
  EXPECT_EQ(1U, list->GetSize());
  Value* tmp_value = NULL;
  ASSERT_TRUE(list->Get(0, &tmp_value));
  EXPECT_TRUE(tmp_value->IsType(Value::TYPE_BOOLEAN));
  bool bool_value = false;
  ASSERT_TRUE(tmp_value->GetAsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);

  // Don't allow empty elements, even if |allow_trailing_comma| is
  // true.
  root.reset(JSONReader::Read("[,]", true));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("[true,,]", true));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("[,true,]", true));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("[true,,false]", true));
  EXPECT_FALSE(root.get());

  // Test objects
  root.reset(JSONReader::Read("{}", false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));

  root.reset(JSONReader::Read(
      "{\"number\":9.87654321, \"null\":null , \"\\x53\" : \"str\" }", false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* dict_val = static_cast<DictionaryValue*>(root.get());
  double_val = 0.0;
  ASSERT_TRUE(dict_val->GetDouble("number", &double_val));
  ASSERT_DOUBLE_EQ(9.87654321, double_val);
  Value* null_val = NULL;
  ASSERT_TRUE(dict_val->Get("null", &null_val));
  ASSERT_TRUE(null_val->IsType(Value::TYPE_NULL));
  str_val.clear();
  ASSERT_TRUE(dict_val->GetString("S", &str_val));
  ASSERT_EQ("str", str_val);

  root2.reset(JSONReader::Read(
      "{\"number\":9.87654321, \"null\":null , \"\\x53\" : \"str\", }", true));
  ASSERT_TRUE(root2.get());
  EXPECT_TRUE(root->Equals(root2.get()));

  // Test newline equivalence.
  root2.reset(JSONReader::Read(
      "{\n"
      "  \"number\":9.87654321,\n"
      "  \"null\":null,\n"
      "  \"\\x53\":\"str\",\n"
      "}\n", true));
  ASSERT_TRUE(root2.get());
  EXPECT_TRUE(root->Equals(root2.get()));

  root2.reset(JSONReader::Read(
      "{\r\n"
      "  \"number\":9.87654321,\r\n"
      "  \"null\":null,\r\n"
      "  \"\\x53\":\"str\",\r\n"
      "}\r\n", true));
  ASSERT_TRUE(root2.get());
  EXPECT_TRUE(root->Equals(root2.get()));

  // Test nesting
  root.reset(JSONReader::Read(
      "{\"inner\":{\"array\":[true]},\"false\":false,\"d\":{}}", false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  dict_val = static_cast<DictionaryValue*>(root.get());
  DictionaryValue* inner_dict = NULL;
  ASSERT_TRUE(dict_val->GetDictionary("inner", &inner_dict));
  ListValue* inner_array = NULL;
  ASSERT_TRUE(inner_dict->GetList("array", &inner_array));
  ASSERT_EQ(1U, inner_array->GetSize());
  bool_value = true;
  ASSERT_TRUE(dict_val->GetBoolean("false", &bool_value));
  ASSERT_FALSE(bool_value);
  inner_dict = NULL;
  ASSERT_TRUE(dict_val->GetDictionary("d", &inner_dict));

  root2.reset(JSONReader::Read(
      "{\"inner\": {\"array\":[true] , },\"false\":false,\"d\":{},}", true));
  EXPECT_TRUE(root->Equals(root2.get()));

  // Test keys with periods
  root.reset(JSONReader::Read(
      "{\"a.b\":3,\"c\":2,\"d.e.f\":{\"g.h.i.j\":1}}", false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  dict_val = static_cast<DictionaryValue*>(root.get());
  int integer_value = 0;
  EXPECT_TRUE(dict_val->GetIntegerWithoutPathExpansion("a.b", &integer_value));
  EXPECT_EQ(3, integer_value);
  EXPECT_TRUE(dict_val->GetIntegerWithoutPathExpansion("c", &integer_value));
  EXPECT_EQ(2, integer_value);
  inner_dict = NULL;
  ASSERT_TRUE(dict_val->GetDictionaryWithoutPathExpansion("d.e.f",
                                                          &inner_dict));
  ASSERT_EQ(1U, inner_dict->size());
  EXPECT_TRUE(inner_dict->GetIntegerWithoutPathExpansion("g.h.i.j",
                                                         &integer_value));
  EXPECT_EQ(1, integer_value);

  root.reset(JSONReader::Read("{\"a\":{\"b\":2},\"a.b\":1}", false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  dict_val = static_cast<DictionaryValue*>(root.get());
  EXPECT_TRUE(dict_val->GetInteger("a.b", &integer_value));
  EXPECT_EQ(2, integer_value);
  EXPECT_TRUE(dict_val->GetIntegerWithoutPathExpansion("a.b", &integer_value));
  EXPECT_EQ(1, integer_value);

  // Invalid, no closing brace
  root.reset(JSONReader::Read("{\"a\": true", false));
  ASSERT_FALSE(root.get());

  // Invalid, keys must be quoted
  root.reset(JSONReader::Read("{foo:true}", false));
  ASSERT_FALSE(root.get());

  // Invalid, trailing comma
  root.reset(JSONReader::Read("{\"a\":true,}", false));
  ASSERT_FALSE(root.get());

  // Invalid, too many commas
  root.reset(JSONReader::Read("{\"a\":true,,\"b\":false}", false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader::Read("{\"a\":true,,\"b\":false}", true));
  ASSERT_FALSE(root.get());

  // Invalid, no separator
  root.reset(JSONReader::Read("{\"a\" \"b\"}", false));
  ASSERT_FALSE(root.get());

  // Invalid, lone comma.
  root.reset(JSONReader::Read("{,}", false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader::Read("{,}", true));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader::Read("{\"a\":true,,}", true));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader::Read("{,\"a\":true}", true));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader::Read("{\"a\":true,,\"b\":false}", true));
  ASSERT_FALSE(root.get());

  // Test stack overflow
  std::string evil(1000000, '[');
  evil.append(std::string(1000000, ']'));
  root.reset(JSONReader::Read(evil, false));
  ASSERT_FALSE(root.get());

  // A few thousand adjacent lists is fine.
  std::string not_evil("[");
  not_evil.reserve(15010);
  for (int i = 0; i < 5000; ++i) {
    not_evil.append("[],");
  }
  not_evil.append("[]]");
  root.reset(JSONReader::Read(not_evil, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root.get());
  ASSERT_EQ(5001U, list->GetSize());

  // Test utf8 encoded input
  root.reset(JSONReader().JsonToValue("\"\xe7\xbd\x91\xe9\xa1\xb5\"",
                                      false, false));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  ASSERT_TRUE(root->GetAsString(&str_val));
  ASSERT_EQ(L"\x7f51\x9875", UTF8ToWide(str_val));

  // Test invalid utf8 encoded input
  root.reset(JSONReader().JsonToValue("\"345\xb0\xa1\xb0\xa2\"",
                                      false, false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader().JsonToValue("\"123\xc0\x81\"",
                                      false, false));
  ASSERT_FALSE(root.get());

  // Test invalid root objects.
  root.reset(JSONReader::Read("null", false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader::Read("true", false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader::Read("10", false));
  ASSERT_FALSE(root.get());
  root.reset(JSONReader::Read("\"root\"", false));
  ASSERT_FALSE(root.get());
}

TEST(JSONReaderTest, ErrorMessages) {
  // Error strings should not be modified in case of success.
  std::string error_message;
  int error_code = 0;
  scoped_ptr<Value> root;
  root.reset(JSONReader::ReadAndReturnError("[42]", false,
                                            &error_code, &error_message));
  EXPECT_TRUE(error_message.empty());
  EXPECT_EQ(0, error_code);

  // Test line and column counting
  const char* big_json = "[\n0,\n1,\n2,\n3,4,5,6 7,\n8,\n9\n]";
  // error here --------------------------------^
  root.reset(JSONReader::ReadAndReturnError(big_json, false,
                                            &error_code, &error_message));
  EXPECT_FALSE(root.get());
  EXPECT_EQ(JSONReader::FormatErrorMessage(5, 9, JSONReader::kSyntaxError),
            error_message);
  EXPECT_EQ(JSONReader::JSON_SYNTAX_ERROR, error_code);

  // Test each of the error conditions
  root.reset(JSONReader::ReadAndReturnError("{},{}", false,
                                            &error_code, &error_message));
  EXPECT_FALSE(root.get());
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 3,
      JSONReader::kUnexpectedDataAfterRoot), error_message);
  EXPECT_EQ(JSONReader::JSON_UNEXPECTED_DATA_AFTER_ROOT, error_code);

  std::string nested_json;
  for (int i = 0; i < 101; ++i) {
    nested_json.insert(nested_json.begin(), '[');
    nested_json.append(1, ']');
  }
  root.reset(JSONReader::ReadAndReturnError(nested_json, false,
                                            &error_code, &error_message));
  EXPECT_FALSE(root.get());
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 101, JSONReader::kTooMuchNesting),
            error_message);
  EXPECT_EQ(JSONReader::JSON_TOO_MUCH_NESTING, error_code);

  root.reset(JSONReader::ReadAndReturnError("42", false,
                                            &error_code, &error_message));
  EXPECT_FALSE(root.get());
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 1,
      JSONReader::kBadRootElementType), error_message);
  EXPECT_EQ(JSONReader::JSON_BAD_ROOT_ELEMENT_TYPE, error_code);

  root.reset(JSONReader::ReadAndReturnError("[1,]", false,
                                            &error_code, &error_message));
  EXPECT_FALSE(root.get());
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 4, JSONReader::kTrailingComma),
            error_message);
  EXPECT_EQ(JSONReader::JSON_TRAILING_COMMA, error_code);

  root.reset(JSONReader::ReadAndReturnError("{foo:\"bar\"}", false,
                                            &error_code, &error_message));
  EXPECT_FALSE(root.get());
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 2,
      JSONReader::kUnquotedDictionaryKey), error_message);
  EXPECT_EQ(JSONReader::JSON_UNQUOTED_DICTIONARY_KEY, error_code);

  root.reset(JSONReader::ReadAndReturnError("{\"foo\":\"bar\",}", false,
                                            &error_code, &error_message));
  EXPECT_FALSE(root.get());
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 14, JSONReader::kTrailingComma),
            error_message);

  root.reset(JSONReader::ReadAndReturnError("[nu]", false,
                                            &error_code, &error_message));
  EXPECT_FALSE(root.get());
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 2, JSONReader::kSyntaxError),
            error_message);
  EXPECT_EQ(JSONReader::JSON_SYNTAX_ERROR, error_code);

  root.reset(JSONReader::ReadAndReturnError("[\"xxx\\xq\"]", false,
                                            &error_code, &error_message));
  EXPECT_FALSE(root.get());
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 7, JSONReader::kInvalidEscape),
            error_message);
  EXPECT_EQ(JSONReader::JSON_INVALID_ESCAPE, error_code);

  root.reset(JSONReader::ReadAndReturnError("[\"xxx\\uq\"]", false,
                                            &error_code, &error_message));
  EXPECT_FALSE(root.get());
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 7, JSONReader::kInvalidEscape),
            error_message);
  EXPECT_EQ(JSONReader::JSON_INVALID_ESCAPE, error_code);

  root.reset(JSONReader::ReadAndReturnError("[\"xxx\\q\"]", false,
                                            &error_code, &error_message));
  EXPECT_FALSE(root.get());
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 7, JSONReader::kInvalidEscape),
            error_message);
  EXPECT_EQ(JSONReader::JSON_INVALID_ESCAPE, error_code);
}

}  // namespace base
