#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace std;

void get_all_filenames(vector<string>& filenames, string& data_list)
{
	ifstream infile(data_list);
	if (!infile)
	{
		cout << "Error: cannot open " << data_list << endl;
		return;
	}

	string line;
	while (std::getline(infile, line))
	{
		filenames.push_back(line);
	}
	infile.close();
}

void save_ply(vector<float>& pts, vector<float>& normals, string filename)
{
	ofstream outfile(filename, ios::binary);
	if (!outfile)
	{
		cout << "Error: cannot open " << filename << endl;
		return;
	}

	// write header
	size_t n = pts.size() / 3;
	outfile << "ply" << endl
		<< "format ascii 1.0" << endl
		<< "element vertex " << n << endl
		<< "property float x" << endl
		<< "property float y" << endl
		<< "property float z" << endl
		<< "property float nx" << endl
		<< "property float ny" << endl
		<< "property float nz" << endl
		<< "element face 0" << endl
		<< "property list uchar int vertex_indices" << endl
		<< "end_header" << endl;

	// wirte contents
	const int len = 128;
	vector<char> buffer(n*len);
	char* pstr = buffer.data();
	for (int i = 0; i < n; ++i)
	{
		sprintf(pstr + i * len,
			"%.6f %.6f %.6f %.6f %.6f %.6f\n",
			pts[3 * i], pts[3 * i + 1], pts[3 * i + 2],
			normals[3 * i], normals[3 * i + 1], normals[3 * i + 2]);
	}

	int k = 0;
	for (int i = 0; i < n; ++i)
	{
		for (int j = len * i; j < len*(i + 1); ++j)
		{
			if (buffer[j] == 0) break;
			buffer[k++] = buffer[j];
		}
	}
	outfile.write(buffer.data(), k);

	outfile.close();
}

void save_label(vector<int>& labels, string filename)
{
	ofstream outfile(filename, ios::binary);
	if (!outfile)
	{
		cout << "Error: cannot open " << filename << endl;
		return;
	}

	string rst;
	for (auto& label : labels)
	{
		rst += to_string(label) + "\n";
	}

	outfile.write(rst.c_str(), rst.size());
	outfile.close();
}

void convert_octree(string& filename_oct, string& filename_pc, bool has_label)
{
	// read octree
	ifstream infile(filename_oct, ios::binary);
	if (!infile)
	{
		cout << "Error: cannot open " << filename_oct << endl;
		return;
	}
	infile.seekg(0, infile.end);
	size_t len = infile.tellg();
	infile.seekg(0, infile.beg);
	vector<char> octree(len);
	infile.read(octree.data(), len);
	infile.close();

	// parse the octree file
	int* octi = (int*)octree.data();
	int total_nnum = octi[0];
	int final_nnum = octi[1];
	int depth = octi[2];
	int full_layer = octi[3];
	const int* node_num = octi + 4;
	const int* node_num_accu = node_num + depth + 1;
	const int* key = node_num_accu + depth + 2;
	const int* children = key + total_nnum;
	const int* data = children + total_nnum;
	const float* normal_ptr = (const float*)data;
	const int* label_ptr = nullptr;
	if (has_label)
	{
		int sz = (2 * depth + 7 + 2 * total_nnum + 4 * final_nnum) * sizeof(int);
		if (octree.size() == sz)
		{
			label_ptr = data + 3 * final_nnum;
		}
		else {
			cout << "Error: " << filename_oct << " is not a valid octree file!\n";
		}
	}	

	// convert to point
	vector<float> pts, normals;
	vector<int> labels;
	pts.reserve(3 * final_nnum);
	normals.reserve(3 * final_nnum);
	if(has_label) labels.reserve(final_nnum);
	children += node_num_accu[depth];
	key += node_num_accu[depth];
	for (int i = 0; i < final_nnum; ++i)
	{
		//if (children[i] != -1)
		{
			unsigned char* pt = (unsigned char*)(key + i);
			for (int c = 0; c < 3; ++c)
			{
				pts.push_back((float)pt[c] + 0.5f);
				normals.push_back(normal_ptr[c*final_nnum + i]);
			}
			if (has_label)
			{
				labels.push_back(label_ptr[i]);
			}
		}
	}
	
	// save
	save_ply(pts, normals, filename_pc);
	if (has_label) save_label(labels, filename_pc + ".label.txt");
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		cout << "Usage: Octree2Ply.exe <input file list> [has_label]" << endl;
		return 0;
	}

	string input_file_list(argv[1]);
	bool has_label = false;
	if (argc > 2) has_label = atoi(argv[2]);

	vector<string> all_files;
	get_all_filenames(all_files, input_file_list);

	#pragma omp parallel for
	for (int i = 0; i < all_files.size(); ++i)
	{
		string filename = all_files[i];
		string filename_ply = filename.substr(0, filename.rfind('.')) + ".ply";

		convert_octree(filename, filename_ply, has_label);

		cout << "Processing: " + filename.substr(filename.rfind('\\') + 1) + "\n";
	}
	return 0;
}