
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ftw.h>
#include <stdint.h>

#include "../shapefile_src/shapefil.h"
#include "mongoose.h"

void list(struct mg_connection *conn, const struct mg_request_info *ri, void *data);
void fields(struct mg_connection *conn, const struct mg_request_info *ri, void *data);
void records(struct mg_connection *conn, const struct mg_request_info *ri, void *data);
void shapes(struct mg_connection *conn, const struct mg_request_info *ri, void *data);
void image(struct mg_connection *conn, const struct mg_request_info *ri, void *data);

const char *port = "2222";

int main(int argc, char **argv)
{
  struct mg_context *ctx = mg_start();
  mg_set_option(ctx, "dir_list", "no");  // Set document root
  int ret = 0;
  ret = mg_set_option(ctx, "ports", "2222");
  while (ret != 1)
  {
    sleep(1);
    ret = mg_set_option(ctx, "ports", "2222");
  }
  mg_set_uri_callback(ctx, "/list", &list, NULL);
  mg_set_uri_callback(ctx, "/fields", &fields, NULL);
  mg_set_uri_callback(ctx, "/records", &records, NULL);
  mg_set_uri_callback(ctx, "/shapes", &shapes, NULL);
  mg_set_uri_callback(ctx, "/image", &image, NULL);
  
  printf("-------------------------------------------------------------------------\n");
  
  for (;;) sleep(10000);
  mg_stop(ctx);
  
}

char data_path[] = "/work/data";
struct mg_connection *dconn = NULL;
static int display_info(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
  if (fpath[ftwbuf->base] == '.') return 0;
  if (strcmp(fpath + strlen(fpath) - 4, ".shp") != 0) return 0;
  
  char file[200];
  strncpy(file, fpath + strlen(data_path), 200);
  file[strlen(file)-4] = 0;
  
  mg_printf(dconn, "<tr><td>%s</td>", file);
  mg_printf(dconn, "<td><a href='/fields?file=%s'>fields</a></td>", file);
  mg_printf(dconn, "<td><a href='/records?file=%s&id=0'>record 0</a></td>", file);
  mg_printf(dconn, "<td><a href='/shapes?file=%s&id=0'>shape 0</a></td>", file);
  mg_printf(dconn, "<td><a href='/image?file=%s&id=0'>image 0</a></td>", file);
  mg_printf(dconn, "<td><a href='/image?file=%s'>full image</a></td>", file);
  mg_printf(dconn, "</tr>\n");
  
  //mg_printf(dconn, "%s (%d) (%s)<br />\n", fpath + strlen(fpath) - 4);
  return 0;           /* To tell nftw() to continue */
}

void list(struct mg_connection *conn, const struct mg_request_info *ri, void *data)
{
  mg_printf(conn, "<h1>Available shapefiles</h1>\n");
  mg_printf(conn, "<table>\n");
  
  dconn = conn;
  int flags = FTW_DEPTH | FTW_PHYS;
  if (nftw(data_path, display_info, 20, flags) == -1) mg_printf(conn, "nftw FAILED\n");
  dconn = NULL;
  
  mg_printf(conn, "</table>\n");
}

void fields(struct mg_connection *conn, const struct mg_request_info *ri, void *data)
{
  char * file = mg_get_var(conn, "file");
  if (file == NULL) { mg_printf(conn, "You need to specify a file."); return; }
  
  char filename[100];
  sprintf(filename, "/Users/kevin/data/%s", file);
  
  DBFHandle d = DBFOpen(filename, "rb");
  if (d == NULL) { mg_printf(conn, "DBFOpen error (%s)\n", filename); return; }
	
  int nRecordCount = DBFGetRecordCount(d);
  int nFieldCount = DBFGetFieldCount(d);
  
  mg_printf(conn, "{\n");
  mg_printf(conn, "  \"file\": \"%s\",\n", file);
  mg_printf(conn, "  \"num_records\": \"%d\",\n", nRecordCount);
  mg_printf(conn, "  \"fields\": {\n");
  for (int i = 0 ; i < nFieldCount ; i++)
  {
    char pszFieldName[12];
    int pnWidth; int pnDecimals;
    char type_names[5][20] = {"string", "integer", "double", "logical", "invalid"};
    
    DBFFieldType ft = DBFGetFieldInfo(d, i, pszFieldName, &pnWidth, &pnDecimals);
    mg_printf(conn, "    \"%d\": {\n", i);
    mg_printf(conn, "      \"name\":\"%s\",\n", pszFieldName);
    mg_printf(conn, "      \"type\":\"%s\",\n", type_names[ft]);
    if (pnWidth != 0) mg_printf(conn, "      \"width\":\"%d\",\n", pnWidth);
    if (pnDecimals != 0) mg_printf(conn, "      \"decimals\":\"%d\"\n", pnDecimals);
    mg_printf(conn, "    }%s\n", (i==nFieldCount-1)?"":",");
  }
  mg_printf(conn, "  }\n");
  mg_printf(conn, "}\n");
	
	if (d != NULL) DBFClose(d);
  free(file);
}

void records(struct mg_connection *conn, const struct mg_request_info *ri, void *data)
{
  char * file = mg_get_var(conn, "file");
  if (file == NULL) { mg_printf(conn, "You need to specify a file."); return; }
  
  char * id_c = mg_get_var(conn, "id");
  if (id_c == NULL) { mg_printf(conn, "You need to specify an id."); return; }
  long id = atoi(id_c);
  free(id_c);
  
  char filename[100];
  sprintf(filename, "/Users/kevin/data/%s", file);
  
  DBFHandle d = DBFOpen(filename, "rb");
  if (d == NULL) { mg_printf(conn, "DBFOpen error (%s)\n", filename); return; }
	
	int nFieldCount = DBFGetFieldCount(d);
  int nRecordCount = DBFGetRecordCount(d);
  
  mg_printf(conn, "{\n");
  mg_printf(conn, "  \"file\": \"%s\",\n", file);
  mg_printf(conn, "  \"num_records\": \"%d\",\n", nRecordCount);
  mg_printf(conn, "  \"record_id\": \"%d\",\n", id);
  mg_printf(conn, "  \"record\": {\n");
  for (int j = 0 ; j < nFieldCount ; j++)
  {
    unsigned int k;
    char pszFieldName[12];
    int pnWidth;
    char *cStr;
    
    DBFFieldType ft = DBFGetFieldInfo(d, j, pszFieldName, &pnWidth, NULL);
    mg_printf(conn, "    \"%d\":{\"field\":\"%s\", \"value\":", j, pszFieldName);
    switch (ft)
    {
      case FTString:
        cStr = (char *)DBFReadStringAttribute(d, id, j);
        for (k = 0 ; k < strlen(cStr) ; k++) if (cStr[k] == '"') cStr[k] = '\'';
        mg_printf(conn, "\"%s\"", cStr);
        break;
      case FTInteger:
        mg_printf(conn, "\"%d\"", DBFReadIntegerAttribute(d, id, j));
        break;
      case FTDouble:
        mg_printf(conn, "\"%f\"", DBFReadDoubleAttribute(d, id, j));
        break;
      case FTLogical:
        break;
      case FTInvalid:
        break;
    }
    mg_printf(conn, "}%s\n", (j==nFieldCount-1)?"":",");
  }
  mg_printf(conn, "  }\n");
  mg_printf(conn, "}\n");
	
	if (d != NULL) DBFClose(d);
  free(file);
}

void shapes(struct mg_connection *conn, const struct mg_request_info *ri, void *data)
{
  char * file = mg_get_var(conn, "file");
  if (file == NULL) { mg_printf(conn, "You need to specify a file."); return; }
  
  char * id_c = mg_get_var(conn, "id");
  if (id_c == NULL) { mg_printf(conn, "You need to specify an id."); return; }
  long id = atoi(id_c);
  free(id_c);
  
  char filename[100];
  sprintf(filename, "/Users/kevin/data/%s", file);
  
	SHPHandle h = SHPOpen(filename, "rb");
  if (h == NULL) { mg_printf(conn, "SHPOpen error (%s)\n", filename); return; }
	
	SHPObject	*psShape = SHPReadObject(h, id);
  int j, iPart;
  
  mg_printf(conn, "{\n");
  mg_printf(conn, "  \"file\": \"%s\",\n", file);
  mg_printf(conn, "  \"record_id\": \"%d\",\n", id);
  mg_printf(conn, "  \"shape_parts\": {\n");
  for (iPart = 0 ; iPart < psShape->nParts ; iPart++)
  {
    int start = psShape->panPartStart[iPart];
    int end = psShape->nVertices;
    
    if (iPart != psShape->nParts - 1) end = psShape->panPartStart[iPart+1];
    
    mg_printf(conn, "    \"%d\":[", iPart);
    for (j = start ; j < end ; j++)
    {
      mg_printf(conn, "[%f,%f]%s", psShape->padfX[j], psShape->padfY[j], (j==end-1)?"":",");
    }
    mg_printf(conn, "]%s\n", (iPart==psShape->nParts-1)?"":",");
  }
  mg_printf(conn, "  }\n");
  mg_printf(conn, "}\n");
  
  if (psShape != NULL) SHPDestroyObject(psShape);
	if (h != NULL) SHPClose(h);
}
  
void image(struct mg_connection *conn, const struct mg_request_info *ri, void *data)
{
  char * file = mg_get_var(conn, "file");
  if (file == NULL) { mg_printf(conn, "You need to specify a file."); return; }
  
  char * id_c = mg_get_var(conn, "id");
  long id = (id_c == NULL) ? -1 : atoi(id_c);
  
  char dbf_filename[200];
  sprintf(dbf_filename, "/Users/kevin/data%s.dbf", file);
  
	SHPHandle h = SHPOpen(dbf_filename, "rb");
  if (h == NULL) { mg_printf(conn, "SHPOpen error (%s)\n", dbf_filename); return; }
  
  DBFHandle d = DBFOpen(dbf_filename, "rb");
  if (d == NULL) { mg_printf(conn, "DBFOpen error (%s)\n", dbf_filename); return; }
  
  //int nRecordCount = DBFGetRecordCount(d);
  
  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
  
  mg_printf(conn, "<a href='/list'>back to list</a><br />\n");
  
  FILE * fp = NULL;
  char image_filename[300];
  sprintf(image_filename, "cache_images%s/%ld", file, id);
  
  char jpg_filename[350];
  sprintf(jpg_filename, "%s.jpg", image_filename);
  fp = fopen(jpg_filename, "r");
  if (fp == NULL)
  {
    char command[1000];
    sprintf(command, "mkdir -p cache_images%s", file);
    system(command);
    
    sprintf(command, "/Users/kevin/work/pipes/read_shapefile %s %ld ", dbf_filename, id);
    
    SHPObject	*psShape = SHPReadObject(h, 0);
    if(psShape->nSHPType == SHPT_POLYGON)
    {
      strcat(command, "| /Users/kevin/work/pipes/tesselate ");
    }
    strcat(command, "| /Users/kevin/work/pipes/add_random_colors ");
    strcat(command, "| /Users/kevin/work/pipes/write_bmp ");
    strcat(command, image_filename);
    strcat(command, ".bmp");
    printf("%s\n", command);
    system(command);
    
    sprintf(command, "convert /Users/kevin/work/shapefile_browser/%s.bmp /Users/kevin/work/shapefile_browser/%s.jpg", image_filename, image_filename);
    printf("%s\n", command);
    system(command);
    
    sprintf(command, "rm /Users/kevin/work/shapefile_browser/%s.bmp", image_filename);
    printf("%s\n", command);
    system(command);
  }
  else fclose(fp);
  
  mg_printf(conn, "<img src='%s.jpg' />", image_filename, image_filename);
  
  if (h != NULL) SHPClose(h);
	if (d != NULL) DBFClose(d);
  free(id_c);
}