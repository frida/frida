project('proj', 'c')

# Subproject overrides 'sub' with another variable than dep2. This should warn.
dependency('sub', fallback : ['sub', 'dep2'])
