[CCode (gir_namespace = "FridaSimmy", gir_version = "1.0")]
namespace Frida.TextPList {
	public class Parser {
		private Lexer lex;
		private Token look;

		public Parser (string text) {
			lex = new Lexer (text);
		}

		public Variant parse () throws Error {
			look = lex.next_token ();

			Variant v = parse_value ();
			if (look.type != EOF)
				throw new Error.PROTOCOL ("Trailing content after root value");
			return v;
		}

		private Variant parse_value () throws Error {
			switch (look.type) {
				case LBRACE: return parse_dict ();
				case LPAREN: return parse_array ();
				case NUMBER: return parse_number ();
				case STRING: return parse_string ();
				default:
					throw new Error.PROTOCOL (@"Unexpected token %s".printf (look.type.to_string ()));
			}
		}

		private Variant parse_dict () throws Error {
			eat (LBRACE);

			var b = new VariantBuilder (new VariantType ("a{sv}"));
			while (look.type != RBRACE) {
				if (look.type != STRING)
					throw new Error.PROTOCOL ("Dictionary key must be STRING");

				string key = look.text;
				eat (STRING);

				eat (EQUAL);

				b.add ("{sv}", key, parse_value ());

				if (look.type == SEMI)
					eat (SEMI);

				if (look.type == RBRACE)
					break;
			}

			eat (RBRACE);

			return b.end ();
		}

		private Variant parse_array () throws Error {
			eat (LPAREN);

			var b = new VariantBuilder (new VariantType ("av"));
			if (look.type != RPAREN) {
				b.add_value (parse_value ());
				while (look.type == COMMA) {
					eat (COMMA);
					b.add_value (new Variant.variant (parse_value ()));
				}
			}

			eat (RPAREN);

			return b.end ();
		}

		private Variant parse_number () throws Error {
			string raw = look.text;
			eat (NUMBER);

			if (raw.index_of_char ('.') != -1) {
				double d;
				if (!double.try_parse (raw, out d))
					throw new Error.PROTOCOL (@"Bad double: %s".printf (raw));
				return d;
			} else {
				int64 x;
				if (!int64.try_parse (raw, out x))
					throw new Error.PROTOCOL (@"Bad int: %s".printf (raw));
				return x;
			}
		}

		private Variant parse_string () throws Error {
			string s = look.text;
			eat (STRING);
			return s;
		}

		private void eat (TokenType t) throws Error {
			if (look.type != t)
				throw new Error.PROTOCOL (@"Expected %s but found %s".printf (t.to_string (), look.type.to_string ()));
			look = lex.next_token ();
		}
	}

	private class Lexer {
		private string s;
		private int i = 0;
		private int n;

		public Lexer (string input) {
			s = input;
			n = s.length;
		}

		public Token next_token () throws Error {
			skip_whitespace_and_comments ();

			if (at_end ())
				return new Token (EOF);

			switch (peek ()) {
				case '{': next (); return new Token (LBRACE);
				case '}': next (); return new Token (RBRACE);
				case '(': next (); return new Token (LPAREN);
				case ')': next (); return new Token (RPAREN);
				case '=': next (); return new Token (EQUAL);
				case ';': next (); return new Token (SEMI);
				case ',': next (); return new Token (COMMA);
				case '"': return new Token (STRING, read_quoted ());
				default:
					var word = read_bare ();
					if (word.length == 0)
						throw new Error.PROTOCOL (@"Unexpected character '%c'".printf (peek ()));
					return read_number_or_bareword (word);
			}
		}

		private bool at_end () {
			return i >= n;
		}

		private char peek () {
			return s[i];
		}

		private char next () {
			return s[i++];
		}

		private void skip_whitespace_and_comments () {
			while (!at_end ()) {
				if (is_whitespace (peek ())) {
					next ();
					continue;
				}

				if (peek () == '/' && i + 1 < n) {
					if (s[i + 1] == '/') {
						i += 2;
						while (!at_end () && peek () != '\n')
							next ();
						continue;
					}

					if (s[i + 1] == '*') {
						i += 2;
						while (!at_end ()) {
							if (peek () == '*' && i + 1 < n && s[i + 1] == '/') {
								i += 2;
								break;
							}
							next ();
						}
						continue;
					}
				}

				break;
			}
		}

		private static bool is_whitespace (char c) {
			return c == ' ' || c == '\t' || c == '\r' || c == '\n';
		}

		private string read_quoted () throws Error {
			next ();

			var sb = new StringBuilder ();
			while (!at_end ()) {
				char c = next ();
				if (c == '\\') {
					if (at_end ())
						break;

					char e = next ();
					switch (e) {
						case 'n':
							sb.append_c ('\n');
							break;
						case 'r':
							sb.append_c ('\r');
							break;
						case 't':
							sb.append_c ('\t');
							break;
						case '"':
							sb.append_c ('"');
							break;
						case '\\':
							sb.append_c ('\\');
							break;
						default:
							sb.append_c (e);
							break;
					}
				} else if (c == '"') {
					return sb.str;
				} else {
					sb.append_c (c);
				}
			}

			throw new Error.PROTOCOL ("Unterminated quoted string");
		}

		private string read_bare () {
			int start = i;
			while (!at_end () && is_bare_char (peek ()))
				next ();
			return s.substring (start, i - start);
		}

		private static bool is_bare_char (char c) {
			if (c.isalnum ())
				return true;
			const string extra = "._-/:$+@";
			return extra.index_of_char (c) != -1;
		}

		private Token read_number_or_bareword (string word) {
			bool numeric = true;
			bool has_dot = false;

			int len = word.length;
			for (int k = 0; k != len; k++) {
				char c = word[k];

				if (k == 0 && (c == '+' || c == '-'))
					continue;

				if (c == '.') {
					if (has_dot) {
						numeric = false;
						break;
					}
					has_dot = true;
					continue;
				}

				if (!c.isdigit ()) {
					numeric = false;
					break;
				}
			}

			if (numeric && word.length > 0 && !(word == "+" || word == "-"))
				return new Token (NUMBER, word);

			return new Token (STRING, word);
		}
	}

	private class Token {
		public TokenType type;
		public string text;

		public Token (TokenType t, string s = "") {
			type = t;
			text = s;
		}
	}

	private enum TokenType {
		LBRACE,
		RBRACE,
		LPAREN,
		RPAREN,
		EQUAL,
		SEMI,
		COMMA,
		STRING,
		NUMBER,
		EOF,
	}
}
