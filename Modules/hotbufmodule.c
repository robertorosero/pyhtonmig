/* ===========================================================================
 * Hotbuf object: an equivalent to Java's NIO ByteBuffer class for fast
 * network I/O.
 */

#include "Python.h"
#include <string.h> /* for memmove */

PyAPI_DATA(PyTypeObject) PyHotbuf_Type;

#define PyHotbuf_Check(op) ((op)->ob_type == &PyHotbuf_Type)

#define Py_END_OF_HOTBUF       (-1)

PyAPI_FUNC(PyObject *) PyHotbuf_New(Py_ssize_t size);



/* ===========================================================================
 * Byte Buffer object implementation
 */


/*
 * hotbuf object structure declaration.

   From the Java Buffer docs:

     A buffer is a linear, finite sequence of elements of a specific
     primitive type. Aside from its content, the essential properties of a
     buffer are its capacity, limit, and position:

       A buffer's capacity is the number of elements it contains. The
       capacity of a buffer is never negative and never changes.

       A buffer's limit is the index of the first element that should not
       be read or written. A buffer's limit is never negative and is never
       greater than its capacity.

       A buffer's position is the index of the next element to be read or
       written. A buffer's position is never negative and is never greater
       than its limit.

   The following invariant holds for the mark, position, limit, and
   capacity values:

      0 <= mark <= position <= limit <= capacity (length)

 */
typedef struct {
    PyObject_HEAD

    /* Base pointer location */
    void*      b_ptr;

    /* Total size in bytes of the area that we can access.  The allocated
       memory must be at least as large as this size. */
    Py_ssize_t b_capacity;

    /*
     * The "active window" is defined by the interval [position, limit[.
     */

    /* The current position in the buffer. */
    int b_position;

    /* The limit position in the buffer. */
    int b_limit;

    /* The mark. From the Java Buffer docs:

         A buffer's mark is the index to which its position will be reset when
         the reset method is invoked. The mark is not always defined, but when
         it is defined it is never negative and is never greater than the
         position. If the mark is defined then it is discarded when the
         position or the limit is adjusted to a value smaller than the mark. If
         the mark is not defined then invoking the reset method causes an
         InvalidMarkException to be thrown.

       The mark is set to -1 to indicate that the mark is unset.
    */
    int b_mark;

} PyHotbufObject;


/*
 * Given a hotbuf object, return the buffer memory (in 'ptr' and 'size') and
 * true if there was no error.
 */

/*
 * Create a new hotbuf where we allocate the memory ourselves.
 */
PyObject *
PyHotbuf_New(Py_ssize_t capacity)
{
    PyObject *o;
    PyHotbufObject * b;

    if (capacity < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "capacity must be zero or positive");
        return NULL;
    }

    /* FIXME: check for overflow in multiply */
    o = (PyObject *)PyObject_MALLOC(sizeof(*b) + capacity);
    if ( o == NULL )
        return PyErr_NoMemory();
    b = (PyHotbufObject *) PyObject_INIT(o, &PyHotbuf_Type);

    /* We setup the memory buffer to be right after the object itself. */
    b->b_ptr = (void *)(b + 1);
    b->b_position = 0;
    b->b_mark = -1;
    b->b_limit = capacity;
    b->b_capacity = capacity;

    return o;
}

/* Methods */

/*
 * Constructor.
 */
static PyObject *
hotbuf_new(PyTypeObject *type, PyObject *args, PyObject *kw)
{
    Py_ssize_t size = -1;

    if (!_PyArg_NoKeywords("hotbuf()", kw))
        return NULL;

    if (!PyArg_ParseTuple(args, "n:hotbuf", &size))
        return NULL;

    if ( size <= 0 ) {
        PyErr_SetString(PyExc_ValueError,
                        "size must be greater than zero");
        return NULL;
    }

    return PyHotbuf_New(size);
}

/*
 * Destructor.
 */
static void
hotbuf_dealloc(PyHotbufObject *self)
{
    /* Note: by virtue of the memory buffer being allocated with the PyObject
       itself, this frees the buffer as well. */
    PyObject_DEL(self);
}

/*
 * Comparison.  We compare the active windows, not the entire allocated buffer
 * memory.
 */
static int
hotbuf_compare(PyHotbufObject *self, PyHotbufObject *other)
{
    Py_ssize_t min_len;
    int cmp;

    min_len = ((self->b_capacity < other->b_capacity) ?
               self->b_capacity : other->b_capacity);
    if (min_len > 0) {
        cmp = memcmp(self->b_ptr, other->b_ptr, min_len);
        if (cmp != 0)
            return cmp;
    }
    return ((self->b_capacity < other->b_capacity) ?
            -1 : (self->b_capacity > other->b_capacity) ? 1 : 0);
}


/*
 * Conversion to 'repr' string.
 */
static PyObject *
hotbuf_repr(PyHotbufObject *self)
{
    return PyString_FromFormat("<hotbuf ptr %p, size %zd at %p>",
                               self->b_ptr,
                               self->b_capacity,
                               self);
}

/*
 * Conversion to string.  We convert only the active window.
 */
static PyObject *
hotbuf_str(PyHotbufObject *self)
{
    return PyString_FromStringAndSize((const char *)self->b_ptr, self->b_capacity);
}



/* ===========================================================================
 * Object Methods (basic interface)
 */

PyDoc_STRVAR(capacity__doc__,
"B.capacity() -> int\n\
\n\
Returns this buffer's capacity. \n\
(the entire size of the allocated buffer.)");

static PyObject*
hotbuf_capacity(PyHotbufObject *self)
{
    return PyInt_FromLong(self->b_capacity);
}


PyDoc_STRVAR(position__doc__,
"B.position() -> int\n\
\n\
Returns this buffer's position.");

static PyObject*
hotbuf_position(PyHotbufObject *self)
{
    return PyInt_FromLong(self->b_position);
}


PyDoc_STRVAR(setposition__doc__,
"B.setposition(int)\n\
\n\
Sets this buffer's position. If the mark is defined and larger than\n\
the new position then it is discarded.  If the given position is\n\
larger than the limit an exception is raised.");

static PyObject*
hotbuf_setposition(PyHotbufObject *self, PyObject* arg)
{
    int newposition;

    newposition = PyInt_AsLong(arg);
    if (newposition == -1 && PyErr_Occurred())
        return NULL;

    if ( newposition > self->b_capacity ) {
        PyErr_SetString(PyExc_IndexError,
                        "position must be smaller than capacity");
        return NULL;
    }

    /* Set the new position */
    self->b_position = newposition;

    /* Discard the mark if it is beyond the new position */
    if ( self->b_mark > self->b_position )
        self->b_mark = -1;

    return Py_None;
}


PyDoc_STRVAR(limit__doc__,
"B.limit() -> int\n\
\n\
Returns this buffer's limit.");

static PyObject*
hotbuf_limit(PyHotbufObject *self)
{
    return PyInt_FromLong(self->b_limit);
}


PyDoc_STRVAR(setlimit__doc__,
"B.setlimit(int)\n\
\n\
Sets this buffer's limit. If the position is larger than the new limit\n\
then it is set to the new limit. If the mark is defined and larger\n\
than the new limit then it is discarded.");

static PyObject*
hotbuf_setlimit(PyHotbufObject *self, PyObject* arg)
{
    int newlimit;

    newlimit = PyInt_AsLong(arg);
    if (newlimit == -1 && PyErr_Occurred())
        return NULL;

    if ( newlimit > self->b_capacity ) {
        PyErr_SetString(PyExc_IndexError,
                        "limit must be smaller than capacity");
        return NULL;
    }

    /* Set the new limit. */
    self->b_limit = newlimit;

    /* If the position is larger than the new limit, set it to the new
       limit. */
    if ( self->b_position > self->b_limit )
        self->b_position = newlimit;

    /* Discard the mark if it is beyond the new limit */
    if ( self->b_mark > self->b_position )
        self->b_mark = -1;

    return Py_None;
}


PyDoc_STRVAR(mark__doc__,
"B.mark() -> int\n\
\n\
Returns this buffer's mark. \n\
Return -1 if the mark is not set.");

static PyObject*
hotbuf_mark(PyHotbufObject *self)
{
    return PyInt_FromLong(self->b_mark);
}


PyDoc_STRVAR(setmark__doc__,
"B.setmark()\n\
\n\
Sets this buffer's mark at its position.");

static PyObject*
hotbuf_setmark(PyHotbufObject *self)
{
    self->b_mark = self->b_position;
    return Py_None;
}


PyDoc_STRVAR(reset__doc__,
"B.reset() -> int\n\
\n\
Resets this buffer's position to the previously-marked position.\n\
Invoking this method neither changes nor discards the mark's value.\n\
An IndexError is raised if the mark has not been set.\n\
This method returns the new position's value.");

static PyObject*
hotbuf_reset(PyHotbufObject *self)
{
    if ( self->b_mark == -1 ) {
        PyErr_SetString(PyExc_IndexError,
                        "mark has not been yet set");
        return NULL;
    }

    self->b_position = self->b_mark;
    return PyInt_FromLong(self->b_position);
}


PyDoc_STRVAR(clear__doc__,
"B.clear()\n\
\n\
Clears this buffer. The position is set to zero, the limit is set to\n\
the capacity, and the mark is discarded.\n\
\n\
Invoke this method before using a sequence of channel-read or put\n\
operations to fill this buffer. For example:\n\
\n\
     buf.clear()     # Prepare buffer for reading\n\
     in.read(buf)    # Read data\n\
\n\
(This method does not actually erase the data in the buffer, but it is\n\
named as if it did because it will most often be used in situations in\n\
which that might as well be the case.)");

static PyObject*
hotbuf_clear(PyHotbufObject *self)
{
    self->b_position = 0;
    self->b_limit = self->b_capacity;
    self->b_mark = -1;
    return Py_None;
}


PyDoc_STRVAR(flip__doc__,
"B.flip()\n\
\n\
Flips this buffer. The limit is set to the current position and then\n\
the position is set to zero. If the mark is defined then it is\n\
discarded.\n\
\n\
After a sequence of channel-read or put operations, invoke this method\n\
to prepare for a sequence of channel-write or relative get\n\
operations. For example:\n\
\n\
     buf.put(magic)    # Prepend header\n\
     in.read(buf)      # Read data into rest of buffer\n\
     buf.flip()        # Flip buffer\n\
     out.write(buf)    # Write header + data to channel\n\
\n\
This method is often used in conjunction with the compact method when\n\
transferring data from one place to another.");

static PyObject*
hotbuf_flip(PyHotbufObject *self)
{
    self->b_limit = self->b_position;
    self->b_position = 0;
    self->b_mark = -1;
    return Py_None;
}


PyDoc_STRVAR(rewind__doc__,
"B.rewind()\n\
\n\
Rewinds this buffer. The position is set to zero and the mark is\n\
discarded.\n\
\n\
Invoke this method before a sequence of channel-write or get\n\
operations, assuming that the limit has already been set\n\
appropriately. For example:\n\
\n\
     out.write(buf)    # Write remaining data\n\
     buf.rewind()      # Rewind buffer\n\
     buf.get(array)    # Copy data into array\n\
");

static PyObject*
hotbuf_rewind(PyHotbufObject *self)
{
    self->b_position = 0;
    self->b_mark = -1;
    return Py_None;
}


PyDoc_STRVAR(remaining__doc__,
"B.remaining() -> int\n\
\n\
Returns the number of bytes between the current position and the limit.");

static PyObject*
hotbuf_remaining(PyHotbufObject *self)
{
    return PyInt_FromLong(self->b_limit - self->b_position);
}



/* ===========================================================================
 * Object Methods (byte buffer interface)
 */

PyDoc_STRVAR(compact__doc__,
"B.compact()\n\
\n\
public abstract ByteBuffer compact()\n\
\n\
Compacts this buffer  (optional operation).\n\
\n\
The bytes between the buffer's current position and its limit, if\n\
any, are copied to the beginning of the buffer. That is, the byte\n\
at index p = position() is copied to index zero, the byte at index\n\
p + 1 is copied to index one, and so forth until the byte at index\n\
limit() - 1 is copied to index n = limit() - 1 - p. The buffer's\n\
position is then set to n+1 and its limit is set to its\n\
capacity. The mark, if defined, is discarded.\n\
\n\
The buffer's position is set to the number of bytes copied, rather\n\
than to zero, so that an invocation of this method can be followed\n\
immediately by an invocation of another relative put method.\n\
\n\
Invoke this method after writing data from a buffer in case the\n\
write was incomplete. The following loop, for example, copies\n\
bytes from one channel to another via the buffer buf:\n\
\n\
     buf.clear()          # Prepare buffer for use\n\
     while 1:\n\
         if in.read(buf) < 0 and buf.remaining() == 0:\n\
             break        # No more bytes to transfer\n\
         buf.flip()\n\
         out.write(buf)\n\
         buf.compact()    # In case of partial write\n\
\n\
");

static PyObject*
hotbuf_compact(PyHotbufObject *self)
{
    int length;

    /* Calculate the number of bytes in the active window */
    length = self->b_limit - self->b_position;

    /* Move the memory from the active window to the beginning of the
       allocated buffer (only if we need to). */
    if ( length > 0 && self->b_position > 0 ) {
        memmove(self->b_ptr, self->b_ptr + self->b_position, length);
    }

    self->b_position = length;
    self->b_limit = self->b_capacity;
    self->b_mark = -1;

    return Py_None;
}



/*

get

public abstract byte get()

    Relative get method. Reads the byte at this buffer's current position, and then increments the position.

    Returns:
        The byte at the buffer's current position 
    Throws:
        BufferUnderflowException - If the buffer's current position is not smaller than its limit

put

public abstract ByteBuffer put(byte b)

    Relative put method  (optional operation).

    Writes the given byte into this buffer at the current position, and then increments the position.

    Parameters:
        b - The byte to be written 
    Returns:
        This buffer 
    Throws:
        BufferOverflowException - If this buffer's current position is not smaller than its limit 
        ReadOnlyBufferException - If this buffer is read-only

get

public abstract byte get(int index)

    Absolute get method. Reads the byte at the given index.

    Parameters:
        index - The index from which the byte will be read 
    Returns:
        The byte at the given index 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit

put

public abstract ByteBuffer put(int index,
                               byte b)

    Absolute put method  (optional operation).

    Writes the given byte into this buffer at the given index.

    Parameters:
        index - The index at which the byte will be written
        b - The byte value to be written 
    Returns:
        This buffer 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit 
        ReadOnlyBufferException - If this buffer is read-only

get

public ByteBuffer get(byte[] dst,
                      int offset,
                      int length)

    Relative bulk get method.

    This method transfers bytes from this buffer into the given destination array. If there are fewer bytes remaining in the buffer than are required to satisfy the request, that is, if length > remaining(), then no bytes are transferred and a BufferUnderflowException is thrown.

    Otherwise, this method copies length bytes from this buffer into the given array, starting at the current position of this buffer and at the given offset in the array. The position of this buffer is then incremented by length.

    In other words, an invocation of this method of the form src.get(dst, off, len) has exactly the same effect as the loop

         for (int i = off; i < off + len; i++)
             dst[i] = src.get(); 

    except that it first checks that there are sufficient bytes in this buffer and it is potentially much more efficient.

    Parameters:
        dst - The array into which bytes are to be written
        offset - The offset within the array of the first byte to be written; must be non-negative and no larger than dst.length
        length - The maximum number of bytes to be written to the given array; must be non-negative and no larger than dst.length - offset 
    Returns:
        This buffer 
    Throws:
        BufferUnderflowException - If there are fewer than length bytes remaining in this buffer 
        IndexOutOfBoundsException - If the preconditions on the offset and length parameters do not hold

get

public ByteBuffer get(byte[] dst)

    Relative bulk get method.

    This method transfers bytes from this buffer into the given destination array. An invocation of this method of the form src.get(a) behaves in exactly the same way as the invocation

         src.get(a, 0, a.length) 

    Returns:
        This buffer 
    Throws:
        BufferUnderflowException - If there are fewer than length bytes remaining in this buffer

put

public ByteBuffer put(ByteBuffer src)

    Relative bulk put method  (optional operation).

    This method transfers the bytes remaining in the given source buffer into this buffer. If there are more bytes remaining in the source buffer than in this buffer, that is, if src.remaining() > remaining(), then no bytes are transferred and a BufferOverflowException is thrown.

    Otherwise, this method copies n = src.remaining() bytes from the given buffer into this buffer, starting at each buffer's current position. The positions of both buffers are then incremented by n.

    In other words, an invocation of this method of the form dst.put(src) has exactly the same effect as the loop

         while (src.hasRemaining())
             dst.put(src.get()); 

    except that it first checks that there is sufficient space in this buffer and it is potentially much more efficient.

    Parameters:
        src - The source buffer from which bytes are to be read; must not be this buffer 
    Returns:
        This buffer 
    Throws:
        BufferOverflowException - If there is insufficient space in this buffer for the remaining bytes in the source buffer 
        IllegalArgumentException - If the source buffer is this buffer 
        ReadOnlyBufferException - If this buffer is read-only

put

public ByteBuffer put(byte[] src,
                      int offset,
                      int length)

    Relative bulk put method  (optional operation).

    This method transfers bytes into this buffer from the given source array. If there are more bytes to be copied from the array than remain in this buffer, that is, if length > remaining(), then no bytes are transferred and a BufferOverflowException is thrown.

    Otherwise, this method copies length bytes from the given array into this buffer, starting at the given offset in the array and at the current position of this buffer. The position of this buffer is then incremented by length.

    In other words, an invocation of this method of the form dst.put(src, off, len) has exactly the same effect as the loop

         for (int i = off; i < off + len; i++)
             dst.put(a[i]); 

    except that it first checks that there is sufficient space in this buffer and it is potentially much more efficient.

    Parameters:
        src - The array from which bytes are to be read
        offset - The offset within the array of the first byte to be read; must be non-negative and no larger than array.length
        length - The number of bytes to be read from the given array; must be non-negative and no larger than array.length - offset 
    Returns:
        This buffer 
    Throws:
        BufferOverflowException - If there is insufficient space in this buffer 
        IndexOutOfBoundsException - If the preconditions on the offset and length parameters do not hold 
        ReadOnlyBufferException - If this buffer is read-only

put

public final ByteBuffer put(byte[] src)

    Relative bulk put method  (optional operation).

    This method transfers the entire content of the given source byte array into this buffer. An invocation of this method of the form dst.put(a) behaves in exactly the same way as the invocation

         dst.put(a, 0, a.length) 

    Returns:
        This buffer 
    Throws:
        BufferOverflowException - If there is insufficient space in this buffer 
        ReadOnlyBufferException - If this buffer is read-only

getChar

public abstract char getChar()

    Relative get method for reading a char value.

    Reads the next two bytes at this buffer's current position, composing them into a char value according to the current byte order, and then increments the position by two.

    Returns:
        The char value at the buffer's current position 
    Throws:
        BufferUnderflowException - If there are fewer than two bytes remaining in this buffer

putChar

public abstract ByteBuffer putChar(char value)

    Relative put method for writing a char value  (optional operation).

    Writes two bytes containing the given char value, in the current byte order, into this buffer at the current position, and then increments the position by two.

    Parameters:
        value - The char value to be written 
    Returns:
        This buffer 
    Throws:
        BufferOverflowException - If there are fewer than two bytes remaining in this buffer 
        ReadOnlyBufferException - If this buffer is read-only

getChar

public abstract char getChar(int index)

    Absolute get method for reading a char value.

    Reads two bytes at the given index, composing them into a char value according to the current byte order.

    Parameters:
        index - The index from which the bytes will be read 
    Returns:
        The char value at the given index 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus one

putChar

public abstract ByteBuffer putChar(int index,
                                   char value)

    Absolute put method for writing a char value  (optional operation).

    Writes two bytes containing the given char value, in the current byte order, into this buffer at the given index.

    Parameters:
        index - The index at which the bytes will be written
        value - The char value to be written 
    Returns:
        This buffer 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus one 
        ReadOnlyBufferException - If this buffer is read-only

asCharBuffer

public abstract CharBuffer asCharBuffer()

    Creates a view of this byte buffer as a char buffer.

    The content of the new buffer will start at this buffer's current position. Changes to this buffer's content will be visible in the new buffer, and vice versa; the two buffers' position, limit, and mark values will be independent.

    The new buffer's position will be zero, its capacity and its limit will be the number of bytes remaining in this buffer divided by two, and its mark will be undefined. The new buffer will be direct if, and only if, this buffer is direct, and it will be read-only if, and only if, this buffer is read-only.

    Returns:
        A new char buffer

getShort

public abstract short getShort()

    Relative get method for reading a short value.

    Reads the next two bytes at this buffer's current position, composing them into a short value according to the current byte order, and then increments the position by two.

    Returns:
        The short value at the buffer's current position 
    Throws:
        BufferUnderflowException - If there are fewer than two bytes remaining in this buffer

putShort

public abstract ByteBuffer putShort(short value)

    Relative put method for writing a short value  (optional operation).

    Writes two bytes containing the given short value, in the current byte order, into this buffer at the current position, and then increments the position by two.

    Parameters:
        value - The short value to be written 
    Returns:
        This buffer 
    Throws:
        BufferOverflowException - If there are fewer than two bytes remaining in this buffer 
        ReadOnlyBufferException - If this buffer is read-only

getShort

public abstract short getShort(int index)

    Absolute get method for reading a short value.

    Reads two bytes at the given index, composing them into a short value according to the current byte order.

    Parameters:
        index - The index from which the bytes will be read 
    Returns:
        The short value at the given index 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus one

putShort

public abstract ByteBuffer putShort(int index,
                                    short value)

    Absolute put method for writing a short value  (optional operation).

    Writes two bytes containing the given short value, in the current byte order, into this buffer at the given index.

    Parameters:
        index - The index at which the bytes will be written
        value - The short value to be written 
    Returns:
        This buffer 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus one 
        ReadOnlyBufferException - If this buffer is read-only

asShortBuffer

public abstract ShortBuffer asShortBuffer()

    Creates a view of this byte buffer as a short buffer.

    The content of the new buffer will start at this buffer's current position. Changes to this buffer's content will be visible in the new buffer, and vice versa; the two buffers' position, limit, and mark values will be independent.

    The new buffer's position will be zero, its capacity and its limit will be the number of bytes remaining in this buffer divided by two, and its mark will be undefined. The new buffer will be direct if, and only if, this buffer is direct, and it will be read-only if, and only if, this buffer is read-only.

    Returns:
        A new short buffer

getInt

public abstract int getInt()

    Relative get method for reading an int value.

    Reads the next four bytes at this buffer's current position, composing them into an int value according to the current byte order, and then increments the position by four.

    Returns:
        The int value at the buffer's current position 
    Throws:
        BufferUnderflowException - If there are fewer than four bytes remaining in this buffer

putInt

public abstract ByteBuffer putInt(int value)

    Relative put method for writing an int value  (optional operation).

    Writes four bytes containing the given int value, in the current byte order, into this buffer at the current position, and then increments the position by four.

    Parameters:
        value - The int value to be written 
    Returns:
        This buffer 
    Throws:
        BufferOverflowException - If there are fewer than four bytes remaining in this buffer 
        ReadOnlyBufferException - If this buffer is read-only

getInt

public abstract int getInt(int index)

    Absolute get method for reading an int value.

    Reads four bytes at the given index, composing them into a int value according to the current byte order.

    Parameters:
        index - The index from which the bytes will be read 
    Returns:
        The int value at the given index 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus three

putInt

public abstract ByteBuffer putInt(int index,
                                  int value)

    Absolute put method for writing an int value  (optional operation).

    Writes four bytes containing the given int value, in the current byte order, into this buffer at the given index.

    Parameters:
        index - The index at which the bytes will be written
        value - The int value to be written 
    Returns:
        This buffer 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus three 
        ReadOnlyBufferException - If this buffer is read-only

asIntBuffer

public abstract IntBuffer asIntBuffer()

    Creates a view of this byte buffer as an int buffer.

    The content of the new buffer will start at this buffer's current position. Changes to this buffer's content will be visible in the new buffer, and vice versa; the two buffers' position, limit, and mark values will be independent.

    The new buffer's position will be zero, its capacity and its limit will be the number of bytes remaining in this buffer divided by four, and its mark will be undefined. The new buffer will be direct if, and only if, this buffer is direct, and it will be read-only if, and only if, this buffer is read-only.

    Returns:
        A new int buffer

getLong

public abstract long getLong()

    Relative get method for reading a long value.

    Reads the next eight bytes at this buffer's current position, composing them into a long value according to the current byte order, and then increments the position by eight.

    Returns:
        The long value at the buffer's current position 
    Throws:
        BufferUnderflowException - If there are fewer than eight bytes remaining in this buffer

putLong

public abstract ByteBuffer putLong(long value)

    Relative put method for writing a long value  (optional operation).

    Writes eight bytes containing the given long value, in the current byte order, into this buffer at the current position, and then increments the position by eight.

    Parameters:
        value - The long value to be written 
    Returns:
        This buffer 
    Throws:
        BufferOverflowException - If there are fewer than eight bytes remaining in this buffer 
        ReadOnlyBufferException - If this buffer is read-only

getLong

public abstract long getLong(int index)

    Absolute get method for reading a long value.

    Reads eight bytes at the given index, composing them into a long value according to the current byte order.

    Parameters:
        index - The index from which the bytes will be read 
    Returns:
        The long value at the given index 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus seven

putLong

public abstract ByteBuffer putLong(int index,
                                   long value)

    Absolute put method for writing a long value  (optional operation).

    Writes eight bytes containing the given long value, in the current byte order, into this buffer at the given index.

    Parameters:
        index - The index at which the bytes will be written
        value - The long value to be written 
    Returns:
        This buffer 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus seven 
        ReadOnlyBufferException - If this buffer is read-only

asLongBuffer

public abstract LongBuffer asLongBuffer()

    Creates a view of this byte buffer as a long buffer.

    The content of the new buffer will start at this buffer's current position. Changes to this buffer's content will be visible in the new buffer, and vice versa; the two buffers' position, limit, and mark values will be independent.

    The new buffer's position will be zero, its capacity and its limit will be the number of bytes remaining in this buffer divided by eight, and its mark will be undefined. The new buffer will be direct if, and only if, this buffer is direct, and it will be read-only if, and only if, this buffer is read-only.

    Returns:
        A new long buffer

getFloat

public abstract float getFloat()

    Relative get method for reading a float value.

    Reads the next four bytes at this buffer's current position, composing them into a float value according to the current byte order, and then increments the position by four.

    Returns:
        The float value at the buffer's current position 
    Throws:
        BufferUnderflowException - If there are fewer than four bytes remaining in this buffer

putFloat

public abstract ByteBuffer putFloat(float value)

    Relative put method for writing a float value  (optional operation).

    Writes four bytes containing the given float value, in the current byte order, into this buffer at the current position, and then increments the position by four.

    Parameters:
        value - The float value to be written 
    Returns:
        This buffer 
    Throws:
        BufferOverflowException - If there are fewer than four bytes remaining in this buffer 
        ReadOnlyBufferException - If this buffer is read-only

getFloat

public abstract float getFloat(int index)

    Absolute get method for reading a float value.

    Reads four bytes at the given index, composing them into a float value according to the current byte order.

    Parameters:
        index - The index from which the bytes will be read 
    Returns:
        The float value at the given index 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus three

putFloat

public abstract ByteBuffer putFloat(int index,
                                    float value)

    Absolute put method for writing a float value  (optional operation).

    Writes four bytes containing the given float value, in the current byte order, into this buffer at the given index.

    Parameters:
        index - The index at which the bytes will be written
        value - The float value to be written 
    Returns:
        This buffer 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus three 
        ReadOnlyBufferException - If this buffer is read-only

asFloatBuffer

public abstract FloatBuffer asFloatBuffer()

    Creates a view of this byte buffer as a float buffer.

    The content of the new buffer will start at this buffer's current position. Changes to this buffer's content will be visible in the new buffer, and vice versa; the two buffers' position, limit, and mark values will be independent.

    The new buffer's position will be zero, its capacity and its limit will be the number of bytes remaining in this buffer divided by four, and its mark will be undefined. The new buffer will be direct if, and only if, this buffer is direct, and it will be read-only if, and only if, this buffer is read-only.

    Returns:
        A new float buffer

getDouble

public abstract double getDouble()

    Relative get method for reading a double value.

    Reads the next eight bytes at this buffer's current position, composing them into a double value according to the current byte order, and then increments the position by eight.

    Returns:
        The double value at the buffer's current position 
    Throws:
        BufferUnderflowException - If there are fewer than eight bytes remaining in this buffer

putDouble

public abstract ByteBuffer putDouble(double value)

    Relative put method for writing a double value  (optional operation).

    Writes eight bytes containing the given double value, in the current byte order, into this buffer at the current position, and then increments the position by eight.

    Parameters:
        value - The double value to be written 
    Returns:
        This buffer 
    Throws:
        BufferOverflowException - If there are fewer than eight bytes remaining in this buffer 
        ReadOnlyBufferException - If this buffer is read-only

getDouble

public abstract double getDouble(int index)

    Absolute get method for reading a double value.

    Reads eight bytes at the given index, composing them into a double value according to the current byte order.

    Parameters:
        index - The index from which the bytes will be read 
    Returns:
        The double value at the given index 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus seven

putDouble

public abstract ByteBuffer putDouble(int index,
                                     double value)

    Absolute put method for writing a double value  (optional operation).

    Writes eight bytes containing the given double value, in the current byte order, into this buffer at the given index.

    Parameters:
        index - The index at which the bytes will be written
        value - The double value to be written 
    Returns:
        This buffer 
    Throws:
        IndexOutOfBoundsException - If index is negative or not smaller than the buffer's limit, minus seven 
        ReadOnlyBufferException - If this buffer is read-only

asDoubleBuffer

public abstract DoubleBuffer asDoubleBuffer()

    Creates a view of this byte buffer as a double buffer.

    The content of the new buffer will start at this buffer's current position. Changes to this buffer's content will be visible in the new buffer, and vice versa; the two buffers' position, limit, and mark values will be independent.

    The new buffer's position will be zero, its capacity and its limit will be the number of bytes remaining in this buffer divided by eight, and its mark will be undefined. The new buffer will be direct if, and only if, this buffer is direct, and it will be read-only if, and only if, this buffer is read-only.

    Returns:
        A new double buffer

*/



/*

order

public final ByteOrder order()

    Retrieves this buffer's byte order.

    The byte order is used when reading or writing multibyte values,
    and when creating buffers that are views of this byte buffer. The
    order of a newly-created byte buffer is always BIG_ENDIAN.

    Returns:
        This buffer's byte order


order

public final ByteBuffer order(ByteOrder bo)

    Modifies this buffer's byte order.

    Parameters:
        bo - The new byte order, either BIG_ENDIAN or LITTLE_ENDIAN 
    Returns:
        This buffer

*/



/* ===========================================================================
 * Buffer protocol methods
 */

/*
 * Returns the buffer for reading or writing.
 */
static Py_ssize_t
hotbuf_getwritebuf(PyHotbufObject *self, Py_ssize_t idx, void **pp)
{
    if ( idx != 0 ) {
        PyErr_SetString(PyExc_SystemError,
                        "accessing non-existent hotbuf segment");
        return -1;
    }

    *pp = self->b_ptr;
    return self->b_capacity;
}

static Py_ssize_t
hotbuf_getsegcount(PyHotbufObject *self, Py_ssize_t *lenp)
{
    if (lenp)
        *lenp = self->b_capacity;
    return 1;
}

static Py_ssize_t
hotbuf_getcharbuf(PyHotbufObject *self, Py_ssize_t idx, const char **pp)
{
    if ( idx != 0 ) {
        PyErr_SetString(PyExc_SystemError,
                        "accessing non-existent hotbuf segment");
        return -1;
    }

    *pp = (const char *)self->b_ptr;
    return self->b_capacity;
}



/* ===========================================================================
 * Sequence methods
 */

static Py_ssize_t
hotbuf_length(PyHotbufObject *self)
{
    assert(self->b_position <= self->b_limit);
    return self->b_limit - self->b_position;
}



/* ===========================================================================
 * Object interfaces declaration
 */


PyDoc_STRVAR(hotbuf_doc,
"hotbuf(capacity) -> hotbuf\n\
\n\
Return a new hotbuf with a buffer of fixed size 'capacity'.\n\
\n\
hotbuf is a C encapsulation of a fixed-size buffer of bytes in memory.\n\
One can read and write objects of different primitive types directly\n\
into it, without having to convert from/to strings.  Also, this is\n\
meant for the network I/O functions (recv, recvfrom, send, sendto) to\n\
read/write directly into without having to create temporary strings.\n\
\n\
Note that hotbuf is a direct Python equivalent of Java's NIO\n\
ByteBuffer class.");


static PyMethodDef
hotbuf_methods[] = {
	{"clear", (PyCFunction)hotbuf_clear, METH_NOARGS, clear__doc__},
	{"capacity", (PyCFunction)hotbuf_capacity, METH_NOARGS, capacity__doc__},
	{"position", (PyCFunction)hotbuf_position, METH_NOARGS, position__doc__},
	{"setposition", (PyCFunction)hotbuf_setposition, METH_O, setposition__doc__},
	{"limit", (PyCFunction)hotbuf_limit, METH_NOARGS, limit__doc__},
	{"setlimit", (PyCFunction)hotbuf_setlimit, METH_O, setlimit__doc__},
	{"mark", (PyCFunction)hotbuf_mark, METH_NOARGS, mark__doc__},
	{"setmark", (PyCFunction)hotbuf_setmark, METH_NOARGS, setmark__doc__},
	{"reset", (PyCFunction)hotbuf_reset, METH_NOARGS, reset__doc__},
	{"flip", (PyCFunction)hotbuf_flip, METH_NOARGS, flip__doc__},
	{"rewind", (PyCFunction)hotbuf_rewind, METH_NOARGS, rewind__doc__},
	{"remaining", (PyCFunction)hotbuf_remaining, METH_NOARGS, remaining__doc__},
	{"compact", (PyCFunction)hotbuf_compact, METH_NOARGS, compact__doc__},
	{NULL, NULL} /* sentinel */
};

static PySequenceMethods hotbuf_as_sequence = {
        (lenfunc)hotbuf_length,                 /*sq_length*/
        0 /* (binaryfunc)hotbuf_concat */,              /*sq_concat*/
        0 /* (ssizeargfunc)hotbuf_repeat */,            /*sq_repeat*/
        0 /* (ssizeargfunc)hotbuf_item */,              /*sq_item*/
        0 /*(ssizessizeargfunc)hotbuf_slice*/,        /*sq_slice*/
        0 /*(ssizeobjargproc)hotbuf_ass_item*/,       /*sq_ass_item*/
        0 /*(ssizessizeobjargproc)hotbuf_ass_slice*/, /*sq_ass_slice*/
};

static PyBufferProcs hotbuf_as_buffer = {
    (readbufferproc)hotbuf_getwritebuf,
    (writebufferproc)hotbuf_getwritebuf,
    (segcountproc)hotbuf_getsegcount,
    (charbufferproc)hotbuf_getcharbuf,
};

PyTypeObject PyHotbuf_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "hotbuf",
    sizeof(PyHotbufObject),
    0,
    (destructor)hotbuf_dealloc,         /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    (cmpfunc)hotbuf_compare,            /* tp_compare */
    (reprfunc)hotbuf_repr,              /* tp_repr */
    0,                                  /* tp_as_number */
    &hotbuf_as_sequence,               /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    (reprfunc)hotbuf_str,              /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    &hotbuf_as_buffer,                 /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    hotbuf_doc,                         /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    hotbuf_methods,                     /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    hotbuf_new,                        /* tp_new */
};



/* ===========================================================================
 * Install Module
 */

PyDoc_STRVAR(module_doc,
             "This module defines an object type which can represent a fixed size\n\
buffer of bytes in momery, from which you can directly read and into\n\
which you can directly write objects in various other types.  This is\n\
used to avoid buffer copies in network I/O as much as possible.  For\n\
example, socket recv() can directly fill a byte buffer's memory and\n\
send() can read the data to be sent from one as well.\n\
\n\
In addition, a byte buffer has two pointers within it, that delimit\n\
an active slice, the current \"position\" and the \"limit\".  The\n\
active region of a byte buffer is located within these boundaries.\n\
\n\
This class is heaviliy inspired from Java's NIO Hotbuffer class.\n\
\n\
The constructor is:\n\
\n\
hotbuf(nbytes) -- create a new hotbuf\n\
");


/* No functions in array module. */
static PyMethodDef a_methods[] = {
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


PyMODINIT_FUNC
inithotbuf(void)
{
    PyObject *m;

    PyHotbuf_Type.ob_type = &PyType_Type;
    m = Py_InitModule3("hotbuf", a_methods, module_doc);
    if (m == NULL)
        return;

    Py_INCREF((PyObject *)&PyHotbuf_Type);
    PyModule_AddObject(m, "HotbufType", (PyObject *)&PyHotbuf_Type);
    Py_INCREF((PyObject *)&PyHotbuf_Type);
    PyModule_AddObject(m, "hotbuf", (PyObject *)&PyHotbuf_Type);
    /* No need to check the error here, the caller will do that */
}



/*
   TODO
   ----
   - Add hash function
   - Add support for sequence methods.
   - Perhaps implement returning the buffer object itself from some of
     the methods in order to allow chaining of operations on a single line.
   - Implement a resize function
   - Maybe remove the API methods declared at the top.
   - Add support for big vs. little endian

   Pending Issues
   --------------
   - Should we support weakrefs?

*/

