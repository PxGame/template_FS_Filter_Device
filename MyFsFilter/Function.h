# ifndef _FUNCTION_H_
# define _FUNCTION_H_
# include <ntifs.h>

ULONG
GetFileFullPathName(
	_In_ PFILE_OBJECT file,
	_Out_ PUNICODE_STRING path
	);

NTSTATUS 
WriteFileHeader(
	_In_ PFILE_OBJECT file,
	_In_ PDEVICE_OBJECT next_dev);

NTSTATUS 
SetFileSize(
	_In_ PDEVICE_OBJECT dev,
	_In_ PFILE_OBJECT file,
	_In_ PLARGE_INTEGER file_size);

NTSTATUS
SetFileInformation(
	_In_ PDEVICE_OBJECT dev,
	_In_ PFILE_OBJECT file,
	_In_ FILE_INFORMATION_CLASS info_class,
	_In_ PFILE_OBJECT set_file,
	_In_ PVOID buf,
	_In_ ULONG buf_Len);

NTSTATUS FileIrpComp(
	_In_ PDEVICE_OBJECT dev,
	_In_ PIRP irp,
	PVOID context
	);

NTSTATUS ReadWriteFile(
	_In_ PDEVICE_OBJECT dev,
	_In_ PFILE_OBJECT file,
	_In_ PLARGE_INTEGER offset,
	_In_ PULONG length,
	_In_ PVOID buffer,
	_In_ BOOLEAN read_write);

//打开预处理器，只有当前进程为加密进程，才需要调用。
ULONG IrpCreatePre(
	_In_ PIRP irp,
	_In_ PIO_STACK_LOCATION irpsp,
	_In_ PFILE_OBJECT file,
	_In_ PDEVICE_OBJECT next_dev);

//用IoCreateFileSpectifyDeviceObjectHint来打开文件。
//这个文件打开后不进入加密链表，所以可以直接Read和Write，不会被加密
HANDLE CreateFileAccordingIrp(
	_In_ PDEVICE_OBJECT dev,
	_In_ PUNICODE_STRING file_full_path,
	_In_ PIO_STACK_LOCATION irpsp,
	_Out_ PNTSTATUS status,
	_Out_ PFILE_OBJECT *file,
	_Out_ PULONG information);

# endif