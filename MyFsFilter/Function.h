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

//��Ԥ��������ֻ�е�ǰ����Ϊ���ܽ��̣�����Ҫ���á�
ULONG IrpCreatePre(
	_In_ PIRP irp,
	_In_ PIO_STACK_LOCATION irpsp,
	_In_ PFILE_OBJECT file,
	_In_ PDEVICE_OBJECT next_dev);

//��IoCreateFileSpectifyDeviceObjectHint�����ļ���
//����ļ��򿪺󲻽�������������Կ���ֱ��Read��Write�����ᱻ����
HANDLE CreateFileAccordingIrp(
	_In_ PDEVICE_OBJECT dev,
	_In_ PUNICODE_STRING file_full_path,
	_In_ PIO_STACK_LOCATION irpsp,
	_Out_ PNTSTATUS status,
	_Out_ PFILE_OBJECT *file,
	_Out_ PULONG information);

# endif