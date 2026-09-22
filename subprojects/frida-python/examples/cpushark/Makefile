all: build

clean:
	rm -rf build dist

build: clean
	python setup.py py2app

dev: clean
	python setup.py py2app -A

devrun: dev
	./dist/CpuShark.app/Contents/MacOS/CpuShark

.PHONY: clean build dev devrun
