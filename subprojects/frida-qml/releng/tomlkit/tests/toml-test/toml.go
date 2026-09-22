package tomltest

import (
	"math"
	"reflect"
)

// cmpTOML consumes the recursive structure of both want and have
// simultaneously. If anything is unequal the result has failed and comparison
// stops.
//
// reflect.DeepEqual could work here, but it won't tell us how the two
// structures are different.
func (r Test) cmpTOML(want, have interface{}) Test {
	if isTomlValue(want) {
		if !isTomlValue(have) {
			return r.fail("Type for key '%s' differs:\n"+
				"  Expected:     %[2]v (%[2]T)\n"+
				"  Your encoder: %[3]v (%[3]T)",
				r.Key, want, have)
		}

		if !deepEqual(want, have) {
			return r.fail("Values for key '%s' differ:\n"+
				"  Expected:     %[2]v (%[2]T)\n"+
				"  Your encoder: %[3]v (%[3]T)",
				r.Key, want, have)
		}
		return r
	}

	switch w := want.(type) {
	case map[string]interface{}:
		return r.cmpTOMLMap(w, have)
	case []interface{}:
		return r.cmpTOMLArrays(w, have)
	default:
		return r.fail("Unrecognized TOML structure: %T", want)
	}
}

func (r Test) cmpTOMLMap(want map[string]interface{}, have interface{}) Test {
	haveMap, ok := have.(map[string]interface{})
	if !ok {
		return r.mismatch("table", want, haveMap)
	}

	// Check that the keys of each map are equivalent.
	for k := range want {
		if _, ok := haveMap[k]; !ok {
			bunk := r.kjoin(k)
			return bunk.fail("Could not find key '%s' in encoder output", bunk.Key)
		}
	}
	for k := range haveMap {
		if _, ok := want[k]; !ok {
			bunk := r.kjoin(k)
			return bunk.fail("Could not find key '%s' in expected output", bunk.Key)
		}
	}

	// Okay, now make sure that each value is equivalent.
	for k := range want {
		if sub := r.kjoin(k).cmpTOML(want[k], haveMap[k]); sub.Failed() {
			return sub
		}
	}
	return r
}

func (r Test) cmpTOMLArrays(want []interface{}, have interface{}) Test {
	// Slice can be decoded to []interface{} for an array of primitives, or
	// []map[string]interface{} for an array of tables.
	//
	// TODO: it would be nicer if it could always decode to []interface{}?
	haveSlice, ok := have.([]interface{})
	if !ok {
		tblArray, ok := have.([]map[string]interface{})
		if !ok {
			return r.mismatch("array", want, have)
		}

		haveSlice = make([]interface{}, len(tblArray))
		for i := range tblArray {
			haveSlice[i] = tblArray[i]
		}
	}

	if len(want) != len(haveSlice) {
		return r.fail("Array lengths differ for key '%s'"+
			"  Expected:     %[2]v (len=%[4]d)\n"+
			"  Your encoder: %[3]v (len=%[5]d)",
			r.Key, want, haveSlice, len(want), len(haveSlice))
	}
	for i := 0; i < len(want); i++ {
		if sub := r.cmpTOML(want[i], haveSlice[i]); sub.Failed() {
			return sub
		}
	}
	return r
}

// reflect.DeepEqual() that deals with NaN != NaN
func deepEqual(want, have interface{}) bool {
	var wantF, haveF float64
	switch f := want.(type) {
	case float32:
		wantF = float64(f)
	case float64:
		wantF = f
	}
	switch f := have.(type) {
	case float32:
		haveF = float64(f)
	case float64:
		haveF = f
	}
	if math.IsNaN(wantF) && math.IsNaN(haveF) {
		return true
	}

	return reflect.DeepEqual(want, have)
}

func isTomlValue(v interface{}) bool {
	switch v.(type) {
	case map[string]interface{}, []interface{}:
		return false
	}
	return true
}
