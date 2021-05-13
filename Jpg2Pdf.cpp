// 
// Jpg2Pdf.cpp : This is a command line tool that generates 1-n jpg files into a pdf
// Author: Zhu Dongning, bluesen.zhu@gmail.com, Shenzhen
// Date: 2021-05-12
// 

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include "Winsock2.h"
#endif

#ifdef __linux__
#include <arpa/inet.h>
#endif

#include <string>
#include <vector>
#include <map>

using namespace std;

const int BUFF_BLOCK_SIZE = 1024 * 4;

// record obj node offset
struct PdfObjRec
{
	int obj_no;
	long offset;

	PdfObjRec() {}
	PdfObjRec(int no, long ofs) : obj_no(no), offset(ofs){}
};

class PdfWriter
{
public:
	PdfWriter(const char* pdfFile);
	~PdfWriter();

	int AddJpgObj(const char* jpgFile);
private:
	int WriteEndObj();

	void WriteImgContentsObj();
	void WriteImgPageObj();
	void RecordObj();

	string pdf_name_;
	FILE* fp_ = NULL;
	long offset_ = 0;   // Current file location

	map<int, PdfObjRec> objs_;
	vector<PdfObjRec> page_set_;

	int img_count_ = 0;
	int img_obj_ = 0;
	int obj_no_ = 0;  // Current obj number
	int parent_obj_no_ = 0;  // parent obj number
};
// ----
void PdfWriter::RecordObj()
{
	fflush(fp_);
	offset_ = ftell(fp_);

	objs_[obj_no_] = PdfObjRec(obj_no_, offset_);
}
// --
void PdfWriter::WriteImgContentsObj() 
{
	obj_no_++;
	RecordObj();		
	fprintf(fp_, "%d 0 obj\r", obj_no_);

	char steam[512];
	snprintf(steam, 512, "q 595.44 0 0 841.68 0.00 0.00 cm 1 g /Im%d Do Q\n", img_count_);
	int len = strlen(steam);
	fprintf(fp_, "<< /Length %d\r"
		">>\r"
		"stream\r"
		"%s"
		"endstream\r"
		"endobj\r", 
		len, steam);
}
// --
void PdfWriter::WriteImgPageObj() 
{
	obj_no_++;
	RecordObj();
	page_set_.push_back(PdfObjRec(obj_no_, offset_));

	fprintf(fp_, "%d 0 obj\r", obj_no_);

	if (parent_obj_no_ == 0) {
		++obj_no_;
		parent_obj_no_ = obj_no_;
	}

	fprintf(fp_, "<<\r"
		"/Type /Page\r"
		"/MediaBox [0 0 596 842]\r"
		"/Parent %d 0 R \r"
		"/Rotate 0 /Resources <<\r"
		"/ProcSet [/PDF /ImageC /ImageB /ImageI]\r"
		"/XObject <<\r"
		"/Im%d %d 0 R\r"
		" >>\r"
		" >>\r"
		"/Contents [ %d 0 R ]\r"
		">>\r"
		"endobj\r",
		parent_obj_no_, img_count_, img_obj_, img_obj_ + 1);
}
// --
int PdfWriter::WriteEndObj()
{
	if (objs_.size() == 0) {
		return(-1);
	}

	char kids[512]= "/Kids [";
	// strcpy(kids, "/Kids [");
	char tmp[50];
	for (auto it : page_set_) {
		snprintf(tmp, 50, " %d 0 R", it.obj_no);
		strcat(kids, tmp);
	}

	fflush(fp_);
	offset_ = ftell(fp_);
	objs_[parent_obj_no_] = PdfObjRec(parent_obj_no_, offset_);

	fprintf(fp_, "%d 0 obj\r", parent_obj_no_);
	fprintf(fp_, "<<\r"
		"/Type /Pages\r"
		"%s]\r"
		"/Count %zd\r"
		">>\r"
		"endobj\r",
		kids, page_set_.size());

	obj_no_++;
	RecordObj();
	fprintf(fp_, "%d 0 obj\r", obj_no_);
	fprintf(fp_, "<<\r"
		"/Type /Catalog\r"
		"/Pages %d 0 R\r"
		">>\r"
		"endobj\r",
		parent_obj_no_);

	obj_no_++;
	RecordObj();
	fprintf(fp_, "%d 0 obj\r", obj_no_);
	fprintf(fp_, "<<\r"
		"<< /Creator ()\r"
		"/CreationDate ()\r"
		"/Author ()\r"
		"/Producer ()\r"
		"/Title ()\r"
		"/Subject ()\r"
		">>\r"
		"endobj\r");

	// write cross reference table
	fflush(fp_);
	offset_ = ftell(fp_);
	fprintf(fp_, "xref\r");
	fprintf(fp_, "1 %zd\r", objs_.size() + 1);
	fprintf(fp_, "0000000000 65535 f\r");
	for (auto& it : objs_) {
		fprintf(fp_, "%010ld 00000 n\r", it.second.offset);
	}

	// last obj:
	fprintf(fp_, "trailer\r");
	fprintf(fp_, "<<\r"
		"/Size %zd\r"
		"/Root %zd 0 R\r"
		"/Info %zd 0 R\r"
		">>\r"
		"startxref\r"
		"%ld\r"
		"%%%%EOF\r",
		objs_.size() + 1, objs_.size() - 1, objs_.size(), offset_);

	fclose(fp_);
	fp_ = NULL;

	printf("startxref=%ld, objs=%zd.\n", offset_, objs_.size());
	printf("Build '%s' OK.\n", pdf_name_.c_str());
	return(1);
}
// --
PdfWriter::PdfWriter(const char* pdfFile)
{
	pdf_name_ = pdfFile;
	fp_ = fopen(pdfFile, "w+b");
	if (fp_ == NULL) {
		printf("create '%s' failed!\n", pdfFile);
		return;
	}

	// pdf head:
	fprintf(fp_, "%%PDF-1.3\r");
}
// --
PdfWriter::~PdfWriter()
{
	WriteEndObj();

	if (fp_)
		fclose(fp_);
}
// --
int PdfWriter::AddJpgObj(const char* jpgFile)
{
	if (fp_ == NULL) {  // fp_: pdf file handle
		return(-1);
	}

	// Open jpg file and read into memory
	FILE* fp = fopen(jpgFile, "rb");
	if (fp == NULL) {
		printf("open '%s' failed!\n", jpgFile);
		return(-2);
	}

	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	if (fsize <= 0) {
		printf("fsize <= 0 : %ld\n", fsize);
		fclose(fp);
		return(-3);
	}

	printf("'%s' fsize=%ld ...\n", jpgFile, fsize);

	char* buff = new char[fsize + 1];
	int n = fsize / BUFF_BLOCK_SIZE;
	long len = 0;
	for (int i = 0; i < n; i++) {
		if (fread(buff + len, BUFF_BLOCK_SIZE, 1, fp) < 1) {
			printf("read err, i=%d, len=%ld\n", i, len);
		}
		len += BUFF_BLOCK_SIZE;
	}
	if(fread(buff + len, fsize - len, 1, fp) < 1) {
		printf("read err, len=%ld, block=%ld\n", len, fsize - len);
	}
	buff[fsize] = '\0';
	fclose(fp);

	// Find the height and width of the picture from the jpg file header
	short int height = 0;
	short int width = 0;
	unsigned char* p0 = (unsigned char*)buff;
	unsigned char* p = p0;
	unsigned char ch, ch2;
	while (1) {
		if (p == p0 + fsize) {
			break;
		}
		ch = *p++;
		ch2 = *p;
		if (ch == 0xFF && ch2 == 0xC0) {
			p += 4;
			memcpy(&height, p, 2);
			height = ntohs(height);
			p += 2;
			memcpy(&width, p, 2);
			width = ntohs(width);
			break;
		}
	}

	if (height == 0 || width == 0) {  // jpg format error
		delete[]buff;

		printf("error\n");
		return(-3);
	}

	obj_no_++;
	img_count_++;

	RecordObj();
	img_obj_ = obj_no_;

	fprintf(fp_, "%d 0 obj\r", obj_no_);
	fprintf(fp_, "<</Type /XObject /Subtype /Image /Name /Im%d " 
		"/Width %d /Height %d /Length %ld /ColorSpace /DeviceRGB /BitsPerComponent 8 "
		"/Filter [ /DCTDecode ] >> stream\r", 
		img_count_, width, height, fsize);

	// write the content of the jpg file:
	len = 0;
	for (int i = 0; i < n; i++) {
		fwrite(buff + len, BUFF_BLOCK_SIZE, 1, fp_);
		len += BUFF_BLOCK_SIZE;
	}
	fwrite(buff + len, fsize - len, 1, fp_);
	
	delete[]buff;

	fprintf(fp_, "endstream\rendobj\r");

	WriteImgContentsObj();
	WriteImgPageObj();

	printf("  ... ... OK.\n\n");
	return(1);
}
// ----
int main(int argc, char* argv[])
{
	printf("Jpg2Pdf v1.00\nCopyright bluesen.zhu@gmail.com, 2021-5-12, Shenzhen\nCombine multiple jpg files into one pdf file\n\n");
	if (argc < 3) {
		printf("usage: jpg2pdf pdf_file jpg1 jpg2 ...\n");
		return(1);
	}

	PdfWriter pdf(argv[1]);
	
	for (int i = 2; i < argc; i++) {
		pdf.AddJpgObj(argv[i]);
	}

}
