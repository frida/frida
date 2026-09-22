project('arithmetic bidmas')

if 5 * 3 - 6 / 2 + 1 != 13
  error('Arithmetic bidmas broken')
endif
if 5 * (3 - 6 / 2) + 1 != 1
  error('Arithmetic bidmas with brackets broken')
endif

if 5 * 12 / 2 * 3 != 90
  error('Sequential multiplication and division broken')
endif
if 5 * (12 / (2 * 3)) != 10
  error('Sequential multiplication and division with brackets broken')
endif
