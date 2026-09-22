pub fn add(a: i32, b: i32) -> i32 {
    a + b
}

#[cfg(test)]
mod tests {
        extern crate helper;

        use super::*;

        // This is an intentinally broken test that should be turned off by extra rust arguments
        #[cfg(not(broken = "false"))]
        #[test]
        fn test_broken() {
                assert_eq!(0, 5);
        }

        #[test]
        fn test_add_sub() {
                let x = helper::subtract(6, 5);
                assert_eq!(add(x, 5), 6);
        }
}
