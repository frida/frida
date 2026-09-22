package tomltest

import (
	"strconv"
	"strings"
	"time"
)

// cmpJSON consumes the recursive structure of both want and have
// simultaneously. If anything is unequal, the result has failed and comparison
// stops.
//
// reflect.DeepEqual could work here, but it won't tell us how the two
// structures are different.
func (r Test) cmpJSON(want, have interface{}) Test {
	switch w := want.(type) {
	case map[string]interface{}:
		return r.cmpJSONMaps(w, have)
	case []interface{}:
		return r.cmpJSONArrays(w, have)
	default:
		return r.fail(
			"Key '%s' in expected output should be a map or a list of maps, but it's a %T",
			r.Key, want)
	}
}

func (r Test) cmpJSONMaps(want map[string]interface{}, have interface{}) Test {
	haveMap, ok := have.(map[string]interface{})
	if !ok {
		return r.mismatch("table", want, haveMap)
	}

	// Check to make sure both or neither are values.
	if isValue(want) && !isValue(haveMap) {
		return r.fail(
			"Key '%s' is supposed to be a value, but the parser reports it as a table",
			r.Key)
	}
	if !isValue(want) && isValue(haveMap) {
		return r.fail(
			"Key '%s' is supposed to be a table, but the parser reports it as a value",
			r.Key)
	}
	if isValue(want) && isValue(haveMap) {
		return r.cmpJSONValues(want, haveMap)
	}

	// Check that the keys of each map are equivalent.
	for k := range want {
		if _, ok := haveMap[k]; !ok {
			bunk := r.kjoin(k)
			return bunk.fail("Could not find key '%s' in parser output.",
				bunk.Key)
		}
	}
	for k := range haveMap {
		if _, ok := want[k]; !ok {
			bunk := r.kjoin(k)
			return bunk.fail("Could not find key '%s' in expected output.",
				bunk.Key)
		}
	}

	// Okay, now make sure that each value is equivalent.
	for k := range want {
		if sub := r.kjoin(k).cmpJSON(want[k], haveMap[k]); sub.Failed() {
			return sub
		}
	}
	return r
}

func (r Test) cmpJSONArrays(want, have interface{}) Test {
	wantSlice, ok := want.([]interface{})
	if !ok {
		return r.bug("'value' should be a JSON array when 'type=array', but it is a %T", want)
	}

	haveSlice, ok := have.([]interface{})
	if !ok {
		return r.fail(
			"Malformed output from your encoder: 'value' is not a JSON array: %T", have)
	}

	if len(wantSlice) != len(haveSlice) {
		return r.fail("Array lengths differ for key '%s':\n"+
			"  Expected:     %d\n"+
			"  Your encoder: %d",
			r.Key, len(wantSlice), len(haveSlice))
	}
	for i := 0; i < len(wantSlice); i++ {
		if sub := r.cmpJSON(wantSlice[i], haveSlice[i]); sub.Failed() {
			return sub
		}
	}
	return r
}

func (r Test) cmpJSONValues(want, have map[string]interface{}) Test {
	wantType, ok := want["type"].(string)
	if !ok {
		return r.bug("'type' should be a string, but it is a %T", want["type"])
	}

	haveType, ok := have["type"].(string)
	if !ok {
		return r.fail("Malformed output from your encoder: 'type' is not a string: %T", have["type"])
	}

	if wantType != haveType {
		return r.valMismatch(wantType, haveType, want, have)
	}

	// If this is an array, then we've got to do some work to check equality.
	if wantType == "array" {
		return r.cmpJSONArrays(want, have)
	}

	// Atomic values are always strings
	wantVal, ok := want["value"].(string)
	if !ok {
		return r.bug("'value' %v should be a string, but it is a %[1]T", want["value"])
	}

	haveVal, ok := have["value"].(string)
	if !ok {
		return r.fail("Malformed output from your encoder: %T is not a string", have["value"])
	}

	// Excepting floats and datetimes, other values can be compared as strings.
	switch wantType {
	case "float":
		return r.cmpFloats(wantVal, haveVal)
	case "datetime", "datetime-local", "date-local", "time-local":
		return r.cmpAsDatetimes(wantType, wantVal, haveVal)
	default:
		return r.cmpAsStrings(wantVal, haveVal)
	}
}

func (r Test) cmpAsStrings(want, have string) Test {
	if want != have {
		return r.fail("Values for key '%s' don't match:\n"+
			"  Expected:     %s\n"+
			"  Your encoder: %s",
			r.Key, want, have)
	}
	return r
}

func (r Test) cmpFloats(want, have string) Test {
	// Special case for NaN, since NaN != NaN.
	if strings.HasSuffix(want, "nan") || strings.HasSuffix(have, "nan") {
		if want != have {
			return r.fail("Values for key '%s' don't match:\n"+
				"  Expected:     %v\n"+
				"  Your encoder: %v",
				r.Key, want, have)
		}
		return r
	}

	wantF, err := strconv.ParseFloat(want, 64)
	if err != nil {
		return r.bug("Could not read '%s' as a float value for key '%s'", want, r.Key)
	}

	haveF, err := strconv.ParseFloat(have, 64)
	if err != nil {
		return r.fail("Malformed output from your encoder: key '%s' is not a float: '%s'", r.Key, have)
	}

	if wantF != haveF {
		return r.fail("Values for key '%s' don't match:\n"+
			"  Expected:     %v\n"+
			"  Your encoder: %v",
			r.Key, wantF, haveF)
	}
	return r
}

var datetimeRepl = strings.NewReplacer(
	" ", "T",
	"t", "T",
	"z", "Z")

var layouts = map[string]string{
	"datetime":       time.RFC3339Nano,
	"datetime-local": "2006-01-02T15:04:05.999999999",
	"date-local":     "2006-01-02",
	"time-local":     "15:04:05",
}

func (r Test) cmpAsDatetimes(kind, want, have string) Test {
	layout, ok := layouts[kind]
	if !ok {
		panic("should never happen")
	}

	wantT, err := time.Parse(layout, datetimeRepl.Replace(want))
	if err != nil {
		return r.bug("Could not read '%s' as a datetime value for key '%s'", want, r.Key)
	}

	haveT, err := time.Parse(layout, datetimeRepl.Replace(want))
	if err != nil {
		return r.fail("Malformed output from your encoder: key '%s' is not a datetime: '%s'", r.Key, have)
	}
	if !wantT.Equal(haveT) {
		return r.fail("Values for key '%s' don't match:\n"+
			"  Expected:     %v\n"+
			"  Your encoder: %v",
			r.Key, wantT, haveT)
	}
	return r
}

func (r Test) cmpAsDatetimesLocal(want, have string) Test {
	if datetimeRepl.Replace(want) != datetimeRepl.Replace(have) {
		return r.fail("Values for key '%s' don't match:\n"+
			"  Expected:     %v\n"+
			"  Your encoder: %v",
			r.Key, want, have)
	}
	return r
}

func (r Test) kjoin(key string) Test {
	if len(r.Key) == 0 {
		r.Key = key
	} else {
		r.Key += "." + key
	}
	return r
}

func isValue(m map[string]interface{}) bool {
	if len(m) != 2 {
		return false
	}
	if _, ok := m["type"]; !ok {
		return false
	}
	if _, ok := m["value"]; !ok {
		return false
	}
	return true
}

func (r Test) mismatch(wantType string, want, have interface{}) Test {
	return r.fail("Key '%s' is not an %s but %[4]T:\n"+
		"  Expected:     %#[3]v\n"+
		"  Your encoder: %#[4]v",
		r.Key, wantType, want, have)
}

func (r Test) valMismatch(wantType, haveType string, want, have interface{}) Test {
	return r.fail("Key '%s' is not an %s but %s:\n"+
		"  Expected:     %#[3]v\n"+
		"  Your encoder: %#[4]v",
		r.Key, wantType, want, have)
}
