pub fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add() {
        assert_eq!(add(1, 2), 3);
    }

    #[test]
    fn test_add_intentional_fail() {
        assert_eq!(add(1, 2), 5);
    }

    #[test]
    #[ignore]
    fn test_add_intentional_fail2() {
        assert_eq!(add(1, 7), 5);
    }
}
