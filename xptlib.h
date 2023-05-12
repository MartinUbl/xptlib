/*
 * Copyright © 2023 Martin Ubl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <bit>
#include <concepts>
#include <variant>
#include <string_view>
#include <unordered_map>
#include <iostream>

namespace xpt {

	// internal namespace - contents are not exposed to the user code
	namespace internal {

#pragma pack(push, 1)

		// header record signatures

		constexpr const char* Header_Signature_Library     = "LIBRARY HEADER RECORD";
		constexpr const char* Header_Signature_Member      = "MEMBER  HEADER RECORD";
		constexpr const char* Header_Signature_Descriptor  = "DSCRPTR HEADER RECORD";
		constexpr const char* Header_Signature_Namestr     = "NAMESTR HEADER RECORD";
		constexpr const char* Header_Signature_Observation = "OBS     HEADER RECORD";

		// internal representation of header signature
		enum class Header_Signature {
			None = 0,

			Library,
			Member,
			Descriptor,
			Namestr,
			Observation
		};

		static const std::unordered_map<std::string_view, Header_Signature> Signature_Map{
			{ Header_Signature_Library, Header_Signature::Library },
			{ Header_Signature_Member, Header_Signature::Member },
			{ Header_Signature_Descriptor, Header_Signature::Descriptor },
			{ Header_Signature_Namestr, Header_Signature::Namestr },
			{ Header_Signature_Observation, Header_Signature::Observation },
		};

		// variable type, matches the actual values of Namestr_Record_1::ntype
		enum class NVar_Type {
			Numeric = 1,	// is a double precision floating-point value encoded in IBM encoding
			String  = 2,	// is an array of ASCII characters
		};

		//ddMMMyy:hh:mm:ss - date and time modified
		struct Date_Time_Record {
			char dtmod_day[2];			// zero-padded day
			char dtmod_month[3];		// 3-character code of a month
			char dtmod_year[2];			// 2 digit year-only - we assume the century to be either 1900 or 2000 depending on context
			char dtmod_colon1[1];
			char dtmod_hour[2];
			char dtmod_colon2[1];
			char dtmod_minute[2];
			char dtmod_colon3[1];
			char dtmod_second[2];
		};

		// file header structure
		struct File_Header_Record {
			char sas_symbol[2][8];		// both fields contain "SAS"
			char saslib[8];				// string, "SASLIB"
			char sasver[8];				// SAS version
			char sas_os[8];				// operating system the SAS was running on when creating this file
			char blanks[24];			// padding (spaces)
			Date_Time_Record created;	// date and time of creation
		};

		// generic header record - is recognized by the namedesc field
		struct Data_Header_Generic {
			char hdrrec[13];		// always contains string "HEADER RECORD"
			char stars[7];			// always filled with stars "*******"
			char namedesc[21];		// always contains right-padded type with spaces, followed by a string "HEADER RECORD"
			char exclamations[7];	// always filled with exclamation marks "!!!!!!!"
			char num1[5];			// always zeroes
			char num2[5];			// NAMESTR - count of variables that follows
			char num3[5];			// always zeroes
			char num4[5];			// MEMBER - unknown, always 00160
			char num5[5];			// always zeroes
			char num6[5];			// MEMBER - size of variable record (140 on classical HW, 136 on VAX/VMS)
		};

		// header of member section
		struct Member_Header_Record {
			char sas_symbol[8];			// always contains "SAS" padded with spaces
			char sas_dsname[8];			// dataset name
			char sasdata[8];			// always contains "SASDATA"
			char sasver[8];				// version of SAS which was used to create this file
			char sas_osname[8];			// operating system the SAS was running on when creating this file
			char blanks[24];			// padding (spaces)
			Date_Time_Record created;	// date and time of creation
		};

		// continuation of the member header
		struct Member_Header_Record_2 {
			Date_Time_Record modified_at;	// last modification date and time
			char padding[16];				// padding
			char dslabel[40];				// label of the dataset
			char dstype[8];					// type of the dataset
		};

		// variable descriptor (called "Namestr" in SAS terminology)
		struct Namestr_Record_1 {
			uint16_t ntype;		// variable type, 1 for numeric, 2 for string
			uint16_t nhfun;		// name hash (always zero, probably unused in this version of format)
			uint16_t nlng;		// length of values of this given variable in observation data
			uint16_t nvar0;		// ordinal number of this variable
			char nname[8];		// variable name (unique)
			char nlabel[40];	// variable label
			char nform[8];		// format name
			uint16_t nfl;		// format field length, 0 if unused
			uint16_t nfd;		// number of decimals in format
			uint16_t nfj;		// value justification, 0 for left, 1 for right
			char nfill[2];		// padding
			char niform[8];		// name of the input format
		};

		// continuation of the variable descriptor
		struct Namestr_Record_2 {
			uint16_t nifl;		// informat length attribute
			uint16_t nifd;		// informat number of decimals
			int32_t npos;		// offset of the variable in observation data
			char rest[52];		// padding
		};

#pragma pack(pop)

		/**
		 * Converts the given value to machine endianness and returns the result
		 * All numerical fields are encoded in big-endian in the XPT file
		 */
		template<std::integral T>
		constexpr T To_Machine_Endian_Raw(T value) {
			if constexpr (std::endian::native == std::endian::big) {
				return value;
			}
			auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
			std::ranges::reverse(value_representation);
			return std::bit_cast<T>(value_representation);
		}

		/**
		 * Converts the given value to machine endianness in-place
		 * All numerical fields are encoded in big-endian in the XPT file
		 */
		template<typename T>
		inline void To_Machine_Endian(T& v) {
			v = To_Machine_Endian_Raw<std::remove_cvref_t<T>>(v);
		}

		/**
		 * Converts the static character array given in the data to string, trimming all blanks from the right
		 */
		template<typename T>
		inline std::string Char_To_String(const T& ca) {
			using TRaw = std::remove_cv_t<T>;
			std::string s{ ca, sizeof(TRaw) };
			s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
				return !std::isspace(ch);
			}).base(), s.end());
			s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
				return !std::isspace(ch);
			}));

			return s;
		}

		/**
		 * Retrieves a numeric value from buffer
		 */
		template<std::integral T>
		inline T Get_From_Buffer(const std::vector<std::byte>& buf, const size_t offset) {
			T dst = static_cast<T>(0);
			std::copy(buf.begin() + offset, buf.begin() + offset + sizeof(T), reinterpret_cast<std::byte*>(&dst));
			return dst;
		}

		/**
		 * Retrieves a string from buffer
		 */
		inline std::string Get_From_Buffer(const std::vector<std::byte>& buf, const size_t offset, const size_t len) {
			std::string dst(len, '\0');
			std::copy(buf.begin() + offset, buf.begin() + offset + len, reinterpret_cast<std::byte*>(dst.data()));
			dst.erase(std::find_if(dst.rbegin(), dst.rend(), [](unsigned char ch) {
				return !std::isspace(ch) && ch != '\0';
			}).base(), dst.end());
			dst.erase(dst.begin(), std::find_if(dst.begin(), dst.end(), [](unsigned char ch) {
				return !std::isspace(ch);
			}));

			return dst;
		}

		/**
		 * Converts the IBM double precision floating-point format to standard IEEE 754
		 *
		 * IBM:  1-bit sign, 7-bits exponent, 56-bits mantissa
		 * IEEE: 1-bit sign, 11-bits exponent, 52-bits mantissa
		 * 
		 * https://en.wikipedia.org/wiki/IBM_hexadecimal_floating-point
		 * http://www.bitsavers.org/pdf/ibm/360/princOps/A22-6821-6_360PrincOpsJan67.pdf
		 * 
		 * input in uint64_t is a raw big-endian representation, read directly from the data
		 */
		double IbmToIEEE(uint64_t raw) {

			const uint64_t in = To_Machine_Endian_Raw(raw);

			const auto sign = in & 0x8000000000000000ULL;
			auto exponent = (in & 0x7f00000000000000ULL) >> 56ULL;
			auto mantissa = in & 0x00ffffffffffffffULL;

			auto shift = 0ULL;
			if ((in & 0x0080000000000000ULL) != 0) {
				shift = 3ULL;
			}
			else if ((in & 0x0040000000000000ULL) != 0) {
				shift = 2ULL;
			}
			else if ((in & 0x0020000000000000ULL) != 0) {
				shift = 1ULL;
			}

			mantissa >>= shift;
			mantissa &= 0xffefffffffffffff;
			exponent -= 65ULL;
			exponent <<= 2ULL;
			exponent = exponent + shift + 1023ULL;
			auto ieee = sign | (exponent << 52ULL) | mantissa;

			return std::bit_cast<double>(ieee);
		}
	}

	enum class NStatus {
		Ok,
		No_Library_Header,
		No_Member_Header,
		No_Descriptor_Header,
		No_Namestr_Header,
		No_Observation_Header,
	};

	// universal transport variant used to export value from internal representation
	using TValue = std::variant<std::string, double>;

	/**
	 * A class representing XPT file loader
	 */
	class File {

		private:
			// input file stream of XPT file
			std::ifstream mFile;

			// record of a variable
			struct Variable_Record {
				std::string name;
				std::string label;
				internal::NVar_Type type;
				size_t length;
				size_t varNum;
				size_t position; // offset in data
			};

			// internal exception for EOF signalization
			class CEOF_Exception : public std::runtime_error {
				public:
					using std::runtime_error::runtime_error;
			};

			// stored variables
			std::vector<Variable_Record> mVariables;

			// actual record ("row") length (so we can properly read the records) - this is a sum of all variable lengths
			size_t mRecord_Len = 0;

		private:
			// read a given structure from file; if padding is enabled, the read is extended to 80 bytes, but only a lower part corresponding to given type is returned
			template<typename T, bool padded = true>
			T Read() {
				const size_t bytes_to_read = (padded ? 80 : sizeof(T));

				// always add padding, even if no padding is requested to simplify things
				struct {
					union {
						T data;
						char padded[80];
					};
				} data_padded;

				// read and check size read
				mFile.read(reinterpret_cast<char*>(&data_padded.data), bytes_to_read);
				if (mFile.gcount() != bytes_to_read)
					throw CEOF_Exception{ "Cannot read requested data" };

				return data_padded.data;
			}

			// reads a given count of bytes into target vector
			void Read(std::vector<std::byte>& target, size_t byte_count) {
				target.resize(byte_count);
				mFile.read(reinterpret_cast<char*>(target.data()), byte_count);
				if (mFile.gcount() != byte_count)
					throw CEOF_Exception{ "Cannot read requested data" };
			}

			// discards a given count of bytes from input stream
			void Read_Discard(size_t count) noexcept {
				mFile.seekg(count, std::ios::cur);
			}

			// recognized data header based on its signature
			static internal::Header_Signature Recognize_Data_Header(const internal::Data_Header_Generic& gen) {
				auto itr = internal::Signature_Map.find(std::string_view{ gen.namedesc, sizeof(gen.namedesc) });
				if (itr == internal::Signature_Map.end()) {
					return internal::Header_Signature::None;
				}

				return itr->second;
			}

			// fetches the column by its ID. The parameter must match the column type, otherwise the column value is converted (an in case of invalid type, an exception may be raised according to standard library rules)
			template<typename Arg0>
			void Fetch_Column_Idx(const std::vector<std::byte>& data, size_t argIdx, Arg0& arg) {

				const auto& mvar = mVariables[argIdx];

				// is the parameter a numeric type (double precision)? fetch number
				if constexpr (std::is_same_v<std::decay_t<Arg0>, double>) {
					if (mvar.type == internal::NVar_Type::Numeric) {
						const auto num_raw = internal::Get_From_Buffer<uint64_t>(data, mvar.position);
						arg = internal::IbmToIEEE(num_raw);
					}
					else {
						arg = std::stod(internal::Get_From_Buffer(data, mvar.position, mvar.length));
					}
				}
				// otherwise fetch a string
				else {
					if (mvar.type == internal::NVar_Type::String) {
						arg = internal::Get_From_Buffer(data, mvar.position, mvar.length);
					}
					else {
						const auto num_raw = internal::Get_From_Buffer<uint64_t>(data, mvar.position);
						arg = std::to_string(internal::IbmToIEEE(num_raw));
					}
				}
			}

			// recursion stop method to fetch the last column
			template<typename Arg0>
			void Fetch_Column(const std::vector<std::byte>& data, const size_t origCnt, Arg0& arg) {
				Fetch_Column_Idx(data, origCnt - 1, arg);
			}

			// variadic method to retrieve columns to given parameters with known types
			template<typename Arg0, typename... Args>
			void Fetch_Column(const std::vector<std::byte>& data, const size_t origCnt, Arg0& arg, Args&... args) {

				const size_t i = origCnt - (sizeof...(Args) + 1);

				Fetch_Column_Idx(data, i, arg);
				Fetch_Column(data, origCnt, args...);
			}

		public:
			File() = default;
			virtual ~File() = default;

			/**
			 * Opens the given file, checks for existence implicitly
			 * Returns true on success, false on failure (file does not exist, insufficient rights, ...)
			 */
			bool Open(const std::filesystem::path& path) {
				mFile.open(path, std::ios::in | std::ios::binary);
				return mFile.is_open();
			}

			/**
			 * Reads the file headers, variables and other meta-information; this must be done prior to Read_Next call
			 * Returns NStatus::Ok on success, or other codes if an error has occurred
			 */
			NStatus Read_Headers() {

				internal::Data_Header_Generic hdr;

				hdr = Read<internal::Data_Header_Generic>();
				if (Recognize_Data_Header(hdr) != internal::Header_Signature::Library) {
					return NStatus::No_Library_Header;
				}

				// We don't really use any of the information provided in these headers for now
				/*auto file_header = */ Read<internal::File_Header_Record>();
				/*auto created_at =  */ Read<internal::Date_Time_Record>();

				hdr = Read<internal::Data_Header_Generic>();
				if (Recognize_Data_Header(hdr) != internal::Header_Signature::Member) {
					return NStatus::No_Member_Header;
				}

				// TODO: parse variable record length from "next" (last 5 bytes - next.num6)

				hdr = Read<internal::Data_Header_Generic>();
				if (Recognize_Data_Header(hdr) != internal::Header_Signature::Descriptor) {
					return NStatus::No_Descriptor_Header;
				}

				// We don't really use any of the information provided in these headers for now
				/*auto member_header_1 = */ Read<internal::Member_Header_Record>();
				/*auto member_header_2 = */ Read<internal::Member_Header_Record_2>();

				hdr = Read<internal::Data_Header_Generic>();

				if (Recognize_Data_Header(hdr) != internal::Header_Signature::Namestr) {
					return NStatus::No_Namestr_Header;
				}

				size_t readCnt = 0;
				mRecord_Len = 0;
				const size_t cnt = std::stoull(std::string{ hdr.num2, 5 });

				// read all variable descriptors ("namestrs") and store them in minimal, internal representation
				for (size_t i = 0; i < cnt; i++) {

					const auto namestr1 = Read<internal::Namestr_Record_1, false>();
					const auto namestr2 = Read<internal::Namestr_Record_2, false>();

					readCnt += sizeof(internal::Namestr_Record_1) + sizeof(internal::Namestr_Record_2);

					auto varLength = static_cast<size_t>(internal::To_Machine_Endian_Raw(namestr1.nlng));

					mVariables.emplace_back(
						internal::Char_To_String(namestr1.nname),
						internal::Char_To_String(namestr1.nlabel),
						static_cast<internal::NVar_Type>(internal::To_Machine_Endian_Raw(namestr1.ntype)),
						varLength,
						static_cast<size_t>(internal::To_Machine_Endian_Raw(namestr1.nvar0)),
						static_cast<size_t>(internal::To_Machine_Endian_Raw(namestr2.npos))
					);

					// increase row length
					mRecord_Len += varLength;
				}

				// padding - discard empty spaces
				const size_t rest = readCnt % 80;
				if (rest != 0) {
					Read_Discard(80 - rest);
				}

				// expect the observation header as last header
				hdr = Read<internal::Data_Header_Generic>();
				if (Recognize_Data_Header(hdr) != internal::Header_Signature::Observation) {
					return NStatus::No_Observation_Header;
				}

				return NStatus::Ok;
			}

			/**
			 * Reads next row and pushes the result to target vector
			 * Returns true on success, false when an EOF occurred (there are no more records in the file)
			 */
			bool Read_Next(std::vector<TValue>& target) {

				target.resize(mVariables.size());

				std::vector<std::byte> data(mRecord_Len);
				try {
					Read(data, mRecord_Len);
				}
				catch (CEOF_Exception&) {
					return false;
				}

				// read the whole row into a vector
				for (size_t i = 0; i < mVariables.size(); i++) {

					const auto& mvar = mVariables[i];

					if (mvar.type == internal::NVar_Type::Numeric) {
						const auto num_raw = internal::Get_From_Buffer<uint64_t>(data, mvar.position);
						target[i] = internal::IbmToIEEE(num_raw);
					}
					else if (mvar.type == internal::NVar_Type::String) {
						target[i] = internal::Get_From_Buffer(data, mvar.position, mvar.length);
					}
				}

				return true;
			}

			/**
			 * Reads next row and stores the result to target parameters of known type
			 * Please note, that if the parameter type does not match the column type, the value is converted by the
			 * internal rule set (standard library std::stod or std::to_string).
			 * Parameters are passed from 0 to N-1, so e.g., if the user specifies only 3 parameters out of 5 columns in total, the last 2 columns are discarded
			 * Returns true on success, false when an EOF occurred (there are no more records in the file)
			 */
			template<typename... Args>
			bool Read_Next(Args&... args) {

				// store the original argument count, to properly assign values from left to right during recursion
				const size_t originalArgCount = sizeof...(Args);

				std::vector<std::byte> data(mRecord_Len);
				try {
					Read(data, mRecord_Len);
				}
				catch (CEOF_Exception&) {
					return false;
				}

				Fetch_Column(data, originalArgCount, args...);

				return true;
			}

			/**
			 * Retrieves a vector of variables
			 */
			const std::vector<Variable_Record>& Get_Variable_Vector() const {
				return mVariables;
			}
	};

}
