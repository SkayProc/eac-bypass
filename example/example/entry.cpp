#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <string>

class driver_t
{
	enum class command_t
	{
		get_base,
		get_peb,
		copy_memory
	};

	struct large_argument_t
	{
		std::uint8_t* from_buffer;
		std::size_t size;
	};

	using NtMapVisualRelativePoints_t = std::uint8_t*( * )( const large_argument_t* const large_argument, const std::uint8_t* const to_buffer, command_t command, std::uint64_t from_process_id, std::uint64_t to_process_id );
	const NtMapVisualRelativePoints_t NtMapVisualRelativePoints = reinterpret_cast< NtMapVisualRelativePoints_t >( GetProcAddress( LoadLibraryA( "win32u.dll" ), "NtMapVisualRelativePoints" ) );

	const std::uint64_t from_process_id = GetCurrentProcessId( ), to_process_id;

public:
	explicit driver_t(const std::uint64_t to_process_id) : to_process_id{ to_process_id } {}

	bool initiate( const char* const window_name );

	template < class type = std::uint8_t* >
	type read( const auto read_address )
	{
		type value;
		
		large_argument_t large_argument{ reinterpret_cast< std::uint8_t* >( read_address ), sizeof( type ) };

		NtMapVisualRelativePoints( &large_argument, reinterpret_cast< std::uint8_t* >( &value ), command_t::copy_memory, to_process_id, from_process_id );

		return value;
	}

	template < class type >
	void write( const auto write_address, const type& value )
	{
		large_argument_t large_argument{ reinterpret_cast< std::uint8_t* >( &value ), sizeof( type ) };

		NtMapVisualRelativePoints( &large_argument, reinterpret_cast< std::uint8_t* >( write_address ), command_t::copy_memory, from_process_id, to_process_id);
	}

	std::uint8_t* base( )
	{
		static const auto base = NtMapVisualRelativePoints( nullptr, nullptr, command_t::get_base, from_process_id, to_process_id );

		return base;
	}

	std::uint8_t* dll(const std::wstring& dll_name)
	{
		static const auto peb = NtMapVisualRelativePoints(nullptr, nullptr, command_t::get_peb, from_process_id, to_process_id);

		if (peb)
		{
			auto v5 = read(peb + 0x18);

			if (v5)
			{
				auto v6 = read(v5 + 16);
				if (v6)
				{
					while (read(v6 + 0x30))
					{
						auto length = read<USHORT>(v6 + 0x58);

						auto start = read(v6 + 0x60);

						std::wstring name{};

						name.reserve(length / 2);

						for (auto i = 0u; i < length / 2; ++i)
						{
							name.push_back(read< WCHAR>(start + i * 2));
						}

						if (name == dll_name)
							return read(v6 + 0x30);

						v6 = read(v6);
						if (!v6)
							return 0;
					}
				}
			}
		}
	}
};

bool driver_t::initiate( const char* const window_name )
{
	return GetWindowThreadProcessId( FindWindowA( nullptr, window_name ), (DWORD*)&to_process_id );
}

int main( )
{

	LoadLibraryA( "user32.dll" );

	driver_t driver{ 0 };

	driver.initiate("Rust");

	std::printf( "base: %p\n", driver.base( ) );
	std::printf( "GameAssembly: %p\n", driver.dll( L"GameAssembly.dll" ) );

	/*std::uint32_t amoy = 0xEAC;
	std::printf( "amoy before: %03X\n", read< std::uint32_t >( &amoy ) );
	write< std::uint32_t >( &amoy, 0xBE );
	std::printf( "amoy after: %02X\n", read< std::uint16_t >( &amoy ) );*/

	return 0;
}
