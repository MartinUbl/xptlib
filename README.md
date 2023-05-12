# xptlib
A fast C++ (C++20) library to read XPT (SAS Xport) file formats. The library streams the data from the file, so the memory does not grow with the file size.

## Requirements

* C++20-compatible compiler and standard library

The library is header-only, so the build system does not matter.

## Examples

First, construct the XPT file reader `xpt::File` and call the `Open` method. Of course you would want to check for the return value.
```
xpt::File file;
if (!file.Open("myfile.xpt")) {
	std::cerr << "Could not open the file!" << std::endl;
	return 1;
}
```
Then, read the headers of the file using the `Read_Headers` method and check for the return code.
```
if (file.Read_Headers() != xpt::NStatus::Ok) {
	std::cerr << "Could not read XPT headers!" << std::endl;
	return 2
}
```
After this, you are able to stream the data from the library using the `Read_Next` method.

The first way is to fetch the data into value vector. This is useful when you don't want to deal with types and allocation by yourself, and just traverse the vector for row values. An example of printing all columns to separate line follows:
```
std::vector<xpt::TValue> values;
while (file.Read_Next(values)) {
	for (auto& v : values) {
		std::visit([&frst, &ofs](auto&& val) {
			std::cout << "Value = " << val << std::endl;
		}, v);
	}
}
```
The second way is to use a variadic template method to fetch the row into separate column values defined by the parameter set. An example of retrieving two strings and two numbers follows:
```
std::string a, b;
double c, d;
while (file.Read_Next(a, b, c, d)) {
	std::cout << a << ", " << b << ", c = " << c << ", d = " << d << std::endl;
}
```
Note, that this is useful when you know the structure of the XPT file. If the parameter types does not match the column types, they are converted according to the standard library rules (`std::stod` for string to number conversion and `std::to_string` for number to string conversion).

You might also want to retrieve the column definitions (e.g., its names and properties) using `Get_Variable_Vector` method call. An example for writing each column name to a separate standard output line follows:
```
auto vars = file.Get_Variable_Vector();
for (auto& v : vars) {
	std::cout << v.name << std::endl;
}
```

## Bugs and feature requests

Feel free to submit an issue, if you found a bug, or if you have a specific feature request worth implementing.

## License

The software is distributed under MIT license. See the attached `LICENSE` file for more information.
