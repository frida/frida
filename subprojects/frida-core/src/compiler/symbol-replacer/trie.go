package main

type trieNode struct {
	children    map[byte]*trieNode
	isEnd       bool
	replacement []byte
}

type trie struct {
	root *trieNode
}

func newTrie() *trie {
	return &trie{
		root: &trieNode{
			children: make(map[byte]*trieNode),
		},
	}
}

func (t *trie) insert(pattern, value []byte) {
	node := t.root
	for _, b := range pattern {
		if node.children[b] == nil {
			node.children[b] = &trieNode{children: make(map[byte]*trieNode)}
		}
		node = node.children[b]
	}
	node.isEnd = true
	node.replacement = value
}

func (t *trie) search(data []byte, start int) (int, []byte, bool) {
	node := t.root
	maxLen := 0
	var match []byte
	for i := start; i < len(data); i++ {
		b := data[i]
		node = node.children[b]
		if node == nil {
			break
		}
		if node.isEnd {
			maxLen = i - start + 1
			match = node.replacement
		}
	}
	if maxLen > 0 {
		return maxLen, match, true
	}
	return 0, nil, false
}
