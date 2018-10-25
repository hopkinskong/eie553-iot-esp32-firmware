files = [
"web_html/login.html",
"web_html/main.html",
"web_html/js/main.js",
"web_html/css/bootstrap.min.css",
"web_html/js/bootstrap.min.js",
"web_html/js/jquery-3.3.1.min.js"
]
output_dir = "main/"


def get_target_path(f):
	return (f.replace("/","_").replace(".","_").replace("-","_"))

for file in files:
	target_path = output_dir + get_target_path(file) + ".h"
	print(file + "  =>  " + target_path)
	i = open(file, "r")
	o = open(target_path, "w")
	data = i.read()

	first=True
	output = "const char " + get_target_path(file) + "[]={"
	for char in data:
		char_code = ord(char)
		if first == True:
			output = output + str(char_code)
			first = False
		else:
			output = output + ', ' + str(char_code)

	output = output + "};"
	o.write(output)
	i.close()
	o.close()

