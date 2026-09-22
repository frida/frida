Prism.languages.meson= {
	'triple-quoted-string': {
		'pattern': /'''[\s\S]*?'''/,
		'alias': 'string'
	},
	'comment': /#.*/,
	'string': /'(?:\\'|[^'])*'/,
	'number': /\b\d+(?:\.\d+)?\b/,
	'keyword': /\b(?:if|else|elif|endif|foreach|endforeach)\b/,
	'function': /(?=\.|\b)[a-zA-Z_]+\s*(?=\()/,
	'boolean': /\b(?:true|false)\b/,
	'builtin': /\b(?:meson|host_machine|target_machine|build_machine)(?=\.)/,
	'operator': /(?:[<>=*+\-/!]?=|%|\/|\*|-|\+|\b(?:or|and|not)\b)/,
	'punctuation': /[(),[\]]/
	// TODO: Handle ternary ?:
};