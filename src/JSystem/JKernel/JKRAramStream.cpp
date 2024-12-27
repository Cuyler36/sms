#include <JSystem/JKernel/JKRAramStream.hpp>
#include <JSystem/JKernel/JKRAramPiece.hpp>
#include <JSystem/JSupport/JSUFileInputStream.hpp>
#include <dolphin/os.h>
#include <macros.h>

JKRAramStream* JKRAramStream::sAramStreamObject;

/* 802B61E4-802B6254       .text create__13JKRAramStreamFl */
JKRAramStream* JKRAramStream::create(s32 priority)
{
	if (!sAramStreamObject) {
		sAramStreamObject = new (JKRGetSystemHeap(), 0) JKRAramStream(priority);
		setTransBuffer(nullptr, 0, nullptr);
	}

	return sAramStreamObject;
}

void* JKRAramStream::sMessageBuffer[4] = { nullptr, nullptr, nullptr, nullptr };
OSMessageQueue JKRAramStream::sMessageQueue = { 0 };

/* 802B6254-802B62A4       .text __ct__13JKRAramStreamFl */
JKRAramStream::JKRAramStream(s32 priority)
    : JKRThread(0x4000, 0x10, priority)
{
	resume();
}

/* 802B62A4-802B6304       .text __dt__13JKRAramStreamFv */
JKRAramStream::~JKRAramStream() { }

/* 802B6304-802B6374       .text run__13JKRAramStreamFv */
void* JKRAramStream::run()
{
	OSInitMessageQueue(&sMessageQueue, sMessageBuffer,
	                   ARRAY_COUNT(sMessageBuffer));

	for (;;) {
		OSMessage message;
		OSReceiveMessage(&sMessageQueue, &message, OS_MESSAGE_BLOCK);
		JKRAramStreamCommand* command = (JKRAramStreamCommand*)message;

		switch (command->mType) {
		case JKRAramStreamCommand::READ:
			readFromAram();
			break;
		case JKRAramStreamCommand::WRITE:
			writeToAram(command);
			break;
		default:
			break;
		}
	}
}

/* 802B6374-802B637C       .text readFromAram__13JKRAramStreamFv */
s32 JKRAramStream::readFromAram() { return 1; }

/* 802B637C-802B6568       .text
 * writeToAram__13JKRAramStreamFP20JKRAramStreamCommand */
s32 JKRAramStream::writeToAram(JKRAramStreamCommand* command)
{
	u32 dstSize       = command->mSize;
	u32 offset        = command->mOffset;
	u32 writtenLength = 0;
	u32 destination   = command->mAddress;
	u8* buffer        = command->mTransferBuffer;
	u32 bufferSize    = command->mTransferBufferSize;
	JKRHeap* heap     = command->mHeap;

	if (buffer) {
		bufferSize = (bufferSize) ? bufferSize : 0x400;

		command->mTransferBufferSize      = bufferSize;
		command->mAllocatedTransferBuffer = false;
	} else {
		bufferSize = (bufferSize) ? bufferSize : 0x400;

		if (heap) {
			buffer = (u8*)JKRAllocFromHeap(heap, bufferSize, -0x20);
			command->mTransferBuffer = buffer;
		} else {
			buffer = (u8*)JKRAllocFromHeap(nullptr, bufferSize, -0x20);
			command->mTransferBuffer = buffer;
		}

		command->mTransferBufferSize      = bufferSize;
		command->mAllocatedTransferBuffer = true;
	}

	if (!buffer) {
		if (!heap) {
			JKRGetCurrentHeap()->dump();
		} else {
			heap->dump();
		}

		OSReport(":::Cannot alloc memory [%s][%d]\n", __FILE__, 168);
		OSPanic(__FILE__, 169, "abort\n");
	}

	if (buffer) {
		command->mStream->seek(offset, SEEK_SET);
		while (dstSize != 0) {
			u32 length = (dstSize > bufferSize) ? bufferSize : dstSize;

			s32 readLength = command->mStream->read(buffer, length);
			if (readLength == 0) {
				writtenLength = 0;
				break;
			}

			JKRAramPcs(0, (u32)buffer, destination, length, nullptr);
			dstSize -= length;
			writtenLength += length;
			destination += length;
		}

		if (command->mAllocatedTransferBuffer) {
			JKRFree(buffer);
			command->mAllocatedTransferBuffer = false;
		}
	}

	OSSendMessage(&command->mMessageQueue, (OSMessage)writtenLength,
	              OS_MESSAGE_NOBLOCK);
	return writtenLength;
}

u8* JKRAramStream::transBuffer;
u32 JKRAramStream::transSize;
JKRHeap* JKRAramStream::transHeap;

/* 802B6568-802B6624       .text
 * write_StreamToAram_Async__13JKRAramStreamFP18JSUFileInputStreamUlUlUl */
JKRAramStreamCommand*
JKRAramStream::write_StreamToAram_Async(JSUFileInputStream* stream, u32 addr,
                                        u32 size, u32 offset)
{
	JKRAramStreamCommand* command
	    = new (JKRGetSystemHeap(), -4) JKRAramStreamCommand();
	command->mType               = JKRAramStreamCommand::WRITE;
	command->mAddress            = addr;
	command->mSize               = size;
	command->mStream             = stream;
	command->field_0x2c          = 0;
	command->mOffset             = offset;
	command->mTransferBuffer     = transBuffer;
	command->mHeap               = transHeap;
	command->mTransferBufferSize = transSize;

	OSInitMessageQueue(&command->mMessageQueue, &command->mMessage, 1);
	OSSendMessage(&sMessageQueue, command, OS_MESSAGE_BLOCK);
	return command;
}

/* 802B6624-802B66B8       .text sync__13JKRAramStreamFP20JKRAramStreamCommandi
 */
JKRAramStreamCommand* JKRAramStream::sync(JKRAramStreamCommand* command,
                                          int isNonBlocking)
{
	OSMessage message;
	if (isNonBlocking == 0) {
		OSReceiveMessage(&command->mMessageQueue, &message, OS_MESSAGE_BLOCK);
		if (message == nullptr) {
			command = nullptr;
			return command;
		} else {
			return command;
		}
	} else {
		BOOL receiveResult = OSReceiveMessage(&command->mMessageQueue, &message,
		                                      OS_MESSAGE_NOBLOCK);
		if (receiveResult == FALSE) {
			command = nullptr;
			return command;
		} else if (message == nullptr) {
			command = nullptr;
			return command;
		} else {
			return command;
		}
	}
}

/* 802B66B8-802B6708       .text setTransBuffer__13JKRAramStreamFPUcUlP7JKRHeap
 */
void JKRAramStream::setTransBuffer(u8* buffer, u32 bufferSize, JKRHeap* heap)
{
	transBuffer = nullptr;
	transSize   = 0x400;
	transHeap   = nullptr;

	if (buffer) {
		transBuffer = (u8*)ALIGN_NEXT((u32)buffer, 0x20);
	}

	if (bufferSize) {
		transSize = ALIGN_PREV(bufferSize, 0x20);
	}

	if (heap && !buffer) {
		transHeap = heap;
	}
}

/* 802B6708-802B6714       .text __ct__20JKRAramStreamCommandFv */
JKRAramStreamCommand::JKRAramStreamCommand()
{
	mAllocatedTransferBuffer = false;
}

static void dummy(JSURandomInputStream* stream) { stream->getAvailable(); }
