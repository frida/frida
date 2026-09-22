#![no_std]

use core::slice::{from_raw_parts, from_raw_parts_mut};

#[repr(C)]
pub struct SearchParameters {
    pub ranges: *const MemoryRange,
    pub num_ranges: usize,

    pub tokens: *const MatchToken,
    pub num_tokens: usize,
}

#[repr(C)]
pub struct SearchResults {
    pub matches: *mut *const u8,
    pub max_matches: usize,
}

#[repr(C)]
pub struct MemoryRange {
    pub base: *const u8,
    pub size: usize,
}

#[repr(C)]
#[derive(PartialEq)]
pub struct MatchToken {
    pub ttype: MatchTokenType,
    pub values: *const u8,
    pub masks: *const u8,
    pub size: usize,
}

#[repr(usize)]
#[derive(PartialEq, Eq)]
pub enum MatchTokenType {
    EXACT,
    WILDCARD,
    MASK,
}

pub fn scan(
    parameters_location: *const SearchParameters,
    results_location: *mut SearchResults,
) -> usize {
    let parameters = unsafe { parameters_location.read() };
    let ranges = unsafe { from_raw_parts(parameters.ranges, parameters.num_ranges) };
    let tokens = unsafe { from_raw_parts(parameters.tokens, parameters.num_tokens) };
    let results = unsafe { results_location.read() };
    let matches = unsafe { from_raw_parts_mut(results.matches, results.max_matches) };

    let mut num_matches = 0;
    let z = TokenCtx::new(tokens);
    for range in ranges {
        let search = MemorySearch::new(unsafe { from_raw_parts(range.base, range.size) }, &z);
        for mtch in search {
            matches[num_matches] = unsafe { range.base.add(mtch) };
            num_matches += 1;
            if num_matches == results.max_matches {
                return num_matches;
            }
        }
    }

    return num_matches;
}

pub struct TokenCtx<'s> {
    pub tokens: &'s [MatchToken],
    pub longest: &'s MatchToken,
    pub longest_values: &'s [u8],
    pub longest_masks: Option<&'s [u8]>,
    pub longest_size: usize,
    pub len_before_longest: usize,
    pub len_from_longest: usize,
    pub total_size: usize,
}
impl<'s> TokenCtx<'s> {
    pub fn new(tokens: &'s [MatchToken]) -> TokenCtx<'s> {
        let longest = match find_longest_token(tokens, MatchTokenType::EXACT) {
            Some(token) => token,
            None => find_longest_token(tokens, MatchTokenType::MASK).unwrap(),
        };

        fn find_longest_token(tokens: &[MatchToken], ttype: MatchTokenType) -> Option<&MatchToken> {
            tokens
                .into_iter()
                .filter(|&t| t.ttype == ttype)
                .max_by_key(|t| t.size)
        }

        let total_size: usize = tokens.into_iter().map(|t| t.size).sum();

        let len_before_longest = tokens
            .split(|t| t == longest)
            .next()
            .unwrap()
            .into_iter()
            .map(|t| t.size)
            .sum();

        return TokenCtx {
            tokens: tokens,
            longest: longest,
            longest_values: unsafe { from_raw_parts(longest.values, longest.size) },
            longest_masks: match longest.ttype {
                MatchTokenType::MASK => {
                    Some(unsafe { from_raw_parts(longest.masks, longest.size) })
                }
                _ => None,
            },
            longest_size: longest.size,
            len_before_longest: len_before_longest,
            total_size: total_size,
            len_from_longest: total_size - len_before_longest,
        };
    }
}

pub struct MemorySearch<'s> {
    pub data: &'s [u8],
    offset: usize,
    remainder: usize,
    pub token_ctx: &'s TokenCtx<'s>,
}
impl<'s> MemorySearch<'s> {
    pub fn new(rrange: &'s [u8], z: &'s TokenCtx) -> MemorySearch<'s> {
        return MemorySearch {
            token_ctx: z,
            data: rrange,
            offset: z.len_before_longest,
            remainder: rrange.len() - z.len_before_longest,
        };
    }
}

impl<'s> Iterator for MemorySearch<'s> {
    type Item = usize;

    fn next(&mut self) -> Option<usize> {
        while self.remainder >= self.token_ctx.len_from_longest {
            let candidate = &self.data[self.offset..self.offset + self.token_ctx.longest.size];

            let is_potential_match = if let Some(masks) = self.token_ctx.longest_masks {
                chunk_matches_values_with_masks(candidate, self.token_ctx.longest_values, masks)
            } else {
                candidate == self.token_ctx.longest_values
            };

            let current_offset = self.offset;

            self.offset += 1;
            self.remainder -= 1;

            if is_potential_match {
                let x = current_offset - self.token_ctx.len_before_longest;
                let is_match = self.token_ctx.tokens.len() == 1 || {
                    let full_candidate = &self.data[x..x + self.token_ctx.total_size];
                    chunk_matches_tokens(full_candidate, self.token_ctx.tokens)
                };
                if is_match {
                    return Some(x);
                }
            }
        }
        None
    }
}

fn chunk_matches_tokens(chunk: &[u8], tokens: &[MatchToken]) -> bool {
    let mut offset = 0;
    for token in tokens {
        if !chunk_matches_token(&chunk[offset..offset + token.size], token) {
            return false;
        }
        offset += token.size
    }
    true
}

fn chunk_matches_token(chunk: &[u8], token: &MatchToken) -> bool {
    let values = unsafe { from_raw_parts(token.values, token.size) };
    match token.ttype {
        MatchTokenType::EXACT => chunk == values,
        MatchTokenType::WILDCARD => true,
        MatchTokenType::MASK => {
            let masks = unsafe { from_raw_parts(token.masks, token.size) };
            chunk_matches_values_with_masks(chunk, values, masks)
        }
    }
}

fn chunk_matches_values_with_masks(chunk: &[u8], values: &[u8], masks: &[u8]) -> bool {
    for (i, byte) in chunk.iter().enumerate() {
        if byte & masks[i] == values[i] {
            return true;
        }
    }
    false
}

#[cfg(test)]
mod tests {
    extern crate alloc;

    use alloc::vec::Vec;
    use core::ptr::null;

    use super::*;

    #[test]
    fn three_exact_matches() {
        let buf: [u8; 7] = [
            0x13, 0x37,
            0x12,
            0x13, 0x37,
            0x13, 0x37
        ];

        let values = [0x13, 0x37];
        let token = make_exact_token(&values);

        let matches = perform_scan(&buf, &[token]);
        assert_eq!(matches.len(), 3);
        assert_eq!(matches[0], 0);
        assert_eq!(matches[1], 3);
        assert_eq!(matches[2], 5);
    }

    #[test]
    fn three_wildcarded_matches() {
        let buf: [u8; 14] = [
            0x12, 0x11, 0x13, 0x37,
            0x12, 0x00,
            0x12, 0xc0, 0x13, 0x37,
            0x12, 0x44, 0x13, 0x37
        ];

        let head: [u8; 1] = [0x12];
        let tail: [u8; 2] = [0x13, 0x37];
        let tokens = [
            make_exact_token(&head),
            make_wildcard_token(1),
            make_exact_token(&tail),
        ];

        let matches = perform_scan(&buf, &tokens);
        assert_eq!(matches.len(), 3);
        assert_eq!(matches[0], 0);
        assert_eq!(matches[1], 6);
        assert_eq!(matches[2], 10);
    }

    #[test]
    fn three_masked_matches() {
        let buf: [u8; 14] = [
            0x12, 0x11, 0x13, 0x35,
            0x12, 0x00,
            0x72, 0xc0, 0x13, 0x37,
            0xb2, 0x44, 0x13, 0x33
        ];

        let a_vals: [u8; 1] = [0x12];
        let a_mask: [u8; 1] = [0x1f];

        let b_vals: [u8; 1] = [0x13];

        let c_vals: [u8; 1] = [0x31];
        let c_mask: [u8; 1] = [0xf1];

        let tokens = [
            make_mask_token(&a_vals, &a_mask),
            make_wildcard_token(1),
            make_exact_token(&b_vals),
            make_mask_token(&c_vals, &c_mask),
        ];

        let matches = perform_scan(&buf, &tokens);
        assert_eq!(matches.len(), 3);
        assert_eq!(matches[0], 0);
        assert_eq!(matches[1], 6);
        assert_eq!(matches[2], 10);
    }

    fn make_exact_token(values: &[u8]) -> MatchToken {
        MatchToken {
            ttype: MatchTokenType::EXACT,
            values: values.as_ptr(),
            masks: null(),
            size: values.len(),
        }
    }

    fn make_wildcard_token(size: usize) -> MatchToken {
        MatchToken {
            ttype: MatchTokenType::WILDCARD,
            values: null(),
            masks: null(),
            size: size,
        }
    }

    fn make_mask_token(values: &[u8], masks: &[u8]) -> MatchToken {
        MatchToken {
            ttype: MatchTokenType::MASK,
            values: values.as_ptr(),
            masks: masks.as_ptr(),
            size: values.len(),
        }
    }

    fn perform_scan(buffer: &[u8], tokens: &[MatchToken]) -> Vec<isize> {
        let range = MemoryRange {
            base: buffer.as_ptr(),
            size: buffer.len(),
        };

        let params = SearchParameters {
            ranges: &range,
            num_ranges: 1,
            tokens: tokens.as_ptr(),
            num_tokens: tokens.len(),
        };

        let mut matches: [*const u8; 4] = [null(), null(), null(), null()];
        let mut results = SearchResults {
            matches: matches.as_mut_ptr(),
            max_matches: matches.len(),
        };

        let num_matches = scan(&params, core::ptr::addr_of_mut!(results));

        matches[..num_matches]
            .into_iter()
            .map(|m| unsafe { m.offset_from(buffer.as_ptr()) })
            .collect()
    }
}
