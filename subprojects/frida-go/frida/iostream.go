package frida

/*#include <frida-core.h>

static void *
read_input_stream(GInputStream *stream, gsize count, gsize *bytes_read, GError **error) {
	void * buffer;
	*bytes_read = g_input_stream_read(stream,buffer,count,NULL,&error);
	return buffer;
}
*/
import "C"
import (
	"io"
	"unsafe"
)

// IOStream type represents struct used to interact with the device using channels.
type IOStream struct {
	stream *C.GIOStream
	input  *C.GInputStream
	output *C.GOutputStream
}

// NewIOStream creates new IOStream.
func NewIOStream(stream *C.GIOStream) *IOStream {
	input := C.g_io_stream_get_input_stream(stream)
	output := C.g_io_stream_get_output_stream(stream)
	return &IOStream{
		stream: stream,
		input:  input,
		output: output,
	}
}

// IsClosed returns whether the stream is closed or not.
func (ios *IOStream) IsClosed() bool {
	closed := C.g_io_stream_is_closed(ios.stream)
	return int(closed) == 1
}

// Close closes the stream.
func (ios *IOStream) Close() error {
	var err *C.GError
	C.g_io_stream_close(ios.stream, nil, &err)
	if err != nil {
		return &FError{err}
	}
	return nil
}

// Read tries to read len(data) bytes into the data from the stream.
func (ios *IOStream) Read(data *[]byte) (int, error) {
	if len(*data) == 0 {
		return 0, nil
	}

	buf := C.CBytes(*data)
	defer C.free(unsafe.Pointer(buf))

	count := C.gsize(len(*data))
	var err *C.GError
	read := C.g_input_stream_read(ios.input,
		unsafe.Pointer(buf),
		count,
		nil,
		&err)
	if err != nil {
		return -1, &FError{err}
	}

	if int(read) == 0 {
		return 0, io.EOF
	}

	dataRead := C.GoBytes(unsafe.Pointer(buf), C.int(read))
	copy(*data, dataRead)

	return int(read), nil
}

// ReadAll tries to read all the bytes provided with the count from the stream.
func (ios *IOStream) ReadAll(count int) ([]byte, error) {
	bt := make([]byte, count)
	buf := C.CBytes(bt)
	defer C.free(unsafe.Pointer(buf))

	countC := C.gsize(count)
	var bytesRead C.gsize
	var err *C.GError
	C.g_input_stream_read_all(
		ios.input,
		buf,
		countC,
		&bytesRead,
		nil,
		&err)
	if err != nil {
		return nil, &FError{err}
	}
	return C.GoBytes(unsafe.Pointer(buf), C.int(bytesRead)), nil
}

// Write tries to write len(data) bytes to the stream.
func (ios *IOStream) Write(data []byte) (int, error) {
	if len(data) == 0 {
		return 0, nil
	}
	buf := C.CBytes(data)
	count := C.gsize(len(data))
	var err *C.GError
	written := C.g_output_stream_write(ios.output,
		buf,
		count,
		nil,
		&err)
	if err != nil {
		return 0, &FError{err}
	}
	return int(written), nil
}

// WriteAll tries to write all the data provided.
func (ios *IOStream) WriteAll(data []byte) error {
	if len(data) == 0 {
		return nil
	}
	buf := C.CBytes(data)
	count := C.gsize(len(data))
	var err *C.GError
	C.g_output_stream_write(ios.output,
		buf,
		count,
		nil,
		&err)
	if err != nil {
		return &FError{err}
	}
	return nil
}

// Clean will clean resources held by the iostream.
func (ios *IOStream) Clean() {
	clean(unsafe.Pointer(ios.stream), unrefGObject)
	clean(unsafe.Pointer(ios.input), unrefGObject)
	clean(unsafe.Pointer(ios.output), unrefGObject)
}
