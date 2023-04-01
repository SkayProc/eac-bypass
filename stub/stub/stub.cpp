#include <ntifs.h>
#include <cstdint>
#include <cstring>

NTSTATUS NTAPI MmCopyVirtualMemory
(
	PEPROCESS SourceProcess,
	PVOID SourceAddress,
	PEPROCESS TargetProcess,
	PVOID TargetAddress,
	SIZE_T BufferSize,
	KPROCESSOR_MODE PreviousMode,
	PSIZE_T ReturnSize
);

enum class command_t
{
	get_base,
	get_peb,
	copy_memory
};

struct large_argument_t
{
	void* from_buffer;
	std::size_t size;
};

template < auto function >
__forceinline auto call( const auto&... arguments )
{
	static void* function_address = nullptr;

	return static_cast< decltype( &function ) >( function_address )( arguments... );
}

//  NtMapVisualRelativePoints
void* stub( large_argument_t* large_argument, void* to_buffer, command_t command, HANDLE from_process_id, HANDLE to_process_id )
{
	void* return_value = nullptr;

	PEPROCESS from_process, to_process;

	if ( !NT_SUCCESS( call< PsLookupProcessByProcessId >( from_process_id, &from_process ) ) )
		return nullptr;
	if ( !NT_SUCCESS( call< PsLookupProcessByProcessId >( to_process_id, &to_process ) ) )
		goto exit;

	switch ( command )
	{
		case command_t::copy_memory:
		{
			std::size_t size;
			call< MmCopyVirtualMemory >( from_process, large_argument->from_buffer, to_process, to_buffer, large_argument->size, UserMode, &size );

			break;
		}
		case command_t::get_base:
		case command_t::get_peb:
		{
			return_value = *reinterpret_cast< void** >( reinterpret_cast< std::uintptr_t >( to_process ) + ( ( command == command_t::get_base ) ? 0x520 : 0x550 ) );

			break;
		}
	}
	
	call< ObfDereferenceObject >( to_process );

exit:
	call< ObfDereferenceObject >( from_process );

	return return_value;
}
