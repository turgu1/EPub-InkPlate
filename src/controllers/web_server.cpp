// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.


#include "global.hpp"

#if EPUB_INKPLATE_BUILD

#include "controllers/wifi.hpp"
#include "viewers/msg_viewer.hpp"
#include "models/config.hpp"
#include "models/page_locs.hpp"

#include <stdio.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>

extern "C" {
  #include <dirent.h>
}

#include "esp_err.h"

#include "esp_vfs.h"
#include "esp_http_server.h"

#include "models/config.hpp"

#define __WEB_SERVER__ 1
#include "web_server.hpp"

static constexpr char const * TAG = "WebServer";

static constexpr int32_t      FILE_PATH_MAX     = 256;
static constexpr int32_t      MAX_FILE_SIZE     = 25*1024*1024;
static constexpr int32_t      SCRATCH_BUFSIZE   = 8192;

#define                       MAX_FILE_SIZE_STR   "25MB"

static httpd_handle_t server = nullptr;

struct FileServerData {
  char base_path[ESP_VFS_PATH_MAX + 1];
  char   scratch[SCRATCH_BUFSIZE];
};

static FileServerData * server_data = nullptr;

// Redirects incoming GET request for /index.html 

static esp_err_t 
index_html_get_handler(httpd_req_t *req)
{
  httpd_resp_set_status(req, "307 Temporary Redirect");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, NULL, 0);  // Response body can be empty
  return ESP_OK;
}

// Respond with an icon file embedded in flash.

static esp_err_t 
favicon_get_handler(httpd_req_t *req)
{
  extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
  extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
  const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
  httpd_resp_set_type(req, "image/x-icon");
  httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
  return ESP_OK;
}

/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path.
 * In case of SPIFFS this returns empty list when path is any
 * string other than '/', since SPIFFS doesn't support directories */
static esp_err_t 
http_resp_dir_html(httpd_req_t *req, const char * dirpath)
{
  char entrysize[16];
  const char * entrytype;

  struct dirent * entry;
  struct stat entry_stat;

  LOG_D("Opening dir: %s.", dirpath);

  const size_t dirpath_len = strlen(dirpath);

  /* Retrieve the base path of file storage to construct the full path */
  std::string entrypath = dirpath;
  entrypath.resize(entrypath.size() - 1);
  DIR * dir = opendir(entrypath.c_str());
  entrypath.append("/");

  if (dir == nullptr) {
    LOG_E("Failed to stat dir : %s (%s)", dirpath, strerror(errno));
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
    return ESP_FAIL;
  }

  httpd_resp_sendstr_chunk(req, 
    "<!DOCTYPE html><html>"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<title>EPub-InkPlate Books Server</title>"
    "<style>"
    "table {font-family: Arial, Helvetica, sans-serif;}"
    "table.list {width: 100%;}"
    "table.list {border-collapse: collapse;}"
    "table.list td {border: 1px solid #ddd; padding: 8px;}"
    "table.list tr:nth-child(even){background-color: #f2f2f2;}"
    "table.list td:nth-child(1), table.list th:nth-child(1){text-align: left;}"
    "table.list td:nth-child(2), table.list th:nth-child(2){text-align: center;}"
    "table.list td:nth-child(3), table.list th:nth-child(3){text-align: right; }"
    "table.list td:nth-child(4), table.list th:nth-child(4){text-align: center;}"
    "table.list th {border: 1px solid #077C95; padding: 12px 8px; background-color: #077C95; color: white;}"
    "table.list tr:hover {background-color: #ddd;}"
    "</style>"
    "<script>"
    "function sortTable(n) {"
      "var table, rows, switching, i, x, y, shouldSwitch, dir;"
      "table = document.getElementById(\"sorted\");"
      "switching = true;"
      "dir = table.getAttribute(\"next_sort\");"
      "while (switching) {"
        "switching = false; rows = table.rows;"
        "for (i = 1; i < (rows.length - 1); i++) {"
          "shouldSwitch = false;"
          "x = rows[i].getElementsByTagName(\"TD\")[n];"
          "y = rows[i + 1].getElementsByTagName(\"TD\")[n];"
          "if (dir == \"asc\") {"
            "if (x.innerHTML.toLowerCase() > y.innerHTML.toLowerCase()) {"
              "shouldSwitch= true; break;"
            "}"
          "} else if (dir == \"desc\") {"
            "if (x.innerHTML.toLowerCase() < y.innerHTML.toLowerCase()) {"
              "shouldSwitch = true; break;"
            "}"
          "}"
        "}"
        "if (shouldSwitch) {"
          "rows[i].parentNode.insertBefore(rows[i + 1], rows[i]);"
          "switching = true;"
        "}"
      "}"
      "table.setAttribute(\"next_sort\", (dir == \"asc\") ? \"desc\" : \"asc\");"
    "}"
    "</script>"
    "</head>"
    "<body>");

  extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
  extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
  const size_t upload_script_size = (upload_script_end - upload_script_start);

  httpd_resp_send_chunk(req, (const char *) upload_script_start, upload_script_size);

  httpd_resp_sendstr_chunk(req,
    "<table class=\"fixed list\" id=\"sorted\" next_sort=\"asc\">"
    "<colgroup><col width=\"70%\"/><col width=\"8%\"/><col width=\"14%\"/><col width=\"8%\"/></colgroup>"
    "<thead><tr><th onclick=\"sortTable(0)\">Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
    "<tbody>");

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(&entry->d_name[strlen(entry->d_name) - 5], ".epub") != 0) continue;
    
    entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

    entrypath.resize(dirpath_len);
    entrypath.append(entry->d_name);

    if (stat(entrypath.c_str(), &entry_stat) == -1) {
      LOG_E("Failed to stat %s : %s", entrytype, entry->d_name);
      continue;
    }

    itoa(entry_stat.st_size, entrysize, 10);
    LOG_I("Found %s : %s (%ld bytes)", entrytype, entry->d_name, entry_stat.st_size);

    std::string the_size;
    uint8_t     length       = strlen(entrysize);
    uint8_t     comma_offset = length % 3;

    for (uint8_t i = 0; i < length; i++) {
      if (((i % 3) == comma_offset) && (i != 0)) {
          the_size += ',';
      }
      the_size += entrysize[i];
    }

    httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
    httpd_resp_sendstr_chunk(req, req->uri);
    httpd_resp_sendstr_chunk(req, entry->d_name);
    if (entry->d_type == DT_DIR) {
      httpd_resp_sendstr_chunk(req, "/");
    }
    httpd_resp_sendstr_chunk(req, "\">");
    httpd_resp_sendstr_chunk(req, entry->d_name);
    httpd_resp_sendstr_chunk(req, "</a></td><td>");
    httpd_resp_sendstr_chunk(req, entrytype);
    httpd_resp_sendstr_chunk(req, "</td><td>");
    httpd_resp_sendstr_chunk(req, the_size.c_str());
    httpd_resp_sendstr_chunk(req, "</td><td>");
    httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete");
    httpd_resp_sendstr_chunk(req, req->uri);
    httpd_resp_sendstr_chunk(req, entry->d_name);
    httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");
  }
  closedir(dir);

  httpd_resp_sendstr_chunk(req,"</tbody></table>");

  httpd_resp_sendstr_chunk(req,
    "<script>window.addEventListener(\"load\", function(){sortTable(0);})</script>" 
    "</body></html>");

  httpd_resp_sendstr_chunk(req, NULL);
  return ESP_OK;
}

inline bool is_file_ext(const std::string & filename, const char * ext) {
    return (filename.substr(filename.size() - sizeof(ext), sizeof(ext)).compare(ext) == 0); }

static esp_err_t 
set_content_type_from_file(httpd_req_t * req, const std::string & filename)
{
  if (is_file_ext(filename, ".epub")) {
    return httpd_resp_set_type(req, "application/epub+zip");
  } 

  return httpd_resp_set_type(req, "text/plain");
}

extern unsigned char hex_to_bin(char ch); 

static std::string 
get_path_from_uri(std::string & dest, const char * base_path, const char * uri)
{
  LOG_D("get_path_from_uri, base_path: %s, uri: %s", base_path, uri);

  const size_t base_pathlen = strlen(base_path);
  size_t pathlen = strlen(uri);

  const char *quest = strchr(uri, '?');
  if (quest) {
    pathlen = MIN(pathlen, quest - uri);
  }
  const char *hash = strchr(uri, '#');
  if (hash) {
    pathlen = MIN(pathlen, hash - uri);
  }

  dest = base_path;
  const char * str_in = uri;
  int           count = pathlen;

  while (count > 0) {
    if (str_in[0] == '%') {
      dest.push_back((char)((hex_to_bin(str_in[1]) << 4) + hex_to_bin(str_in[2])));
      count  -= 3;
      str_in += 3;
    }
    else {
      dest.push_back(*str_in++);
      count--;
    }
  }

  return dest.substr(base_pathlen);
}

// ----- download_handler() -----

static esp_err_t
download_handler(httpd_req_t * req)
{
  LOG_D("download_handler(%s) %d", req->uri, (int) req->user_ctx);

  FILE * fd = nullptr;
  struct stat file_stat;

  std::string filepath;
  std::string filename = get_path_from_uri(filepath, 
                                           ((FileServerData *) req->user_ctx)->base_path,
                                           req->uri);

  LOG_D("filepath = %s, filename = %s", filepath.c_str(), filename.c_str());

  if (filename.back() == '/') {
    return http_resp_dir_html(req, filepath.c_str());
  }

  if (stat(filepath.c_str(), &file_stat) == -1) {
    LOG_D("Filename: %s", filename.c_str());
      /* If file not present check if URL
        * corresponds to one of the hardcoded paths */
    if (filename.compare("/index.html") == 0) {
      return index_html_get_handler(req);
    } else if (filename.compare("/favicon.ico") == 0) {
      return favicon_get_handler(req);
    }
    LOG_E("Failed to stat file : %s", filepath.c_str());
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
    return ESP_FAIL;
  }

  fd = fopen(filepath.c_str(), "r");
  if (!fd) {
    LOG_E("Failed to read existing file : %s", filepath.c_str());
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
    return ESP_FAIL;
  }

  LOG_I("Sending file : %s (%ld bytes)...", filename.c_str(), file_stat.st_size);
  set_content_type_from_file(req, filename);

  // Retrieve the pointer to scratch buffer for temporary storage
  char *chunk = ((FileServerData *)req->user_ctx)->scratch;
  size_t chunksize;
  do {
    chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

    if (chunksize > 0) {
      if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
        fclose(fd);
        LOG_E("File sending failed!");
        httpd_resp_sendstr_chunk(req, NULL);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
        return ESP_FAIL;
      }
    }
  } while (chunksize != 0);

  fclose(fd);
  LOG_I("File sending complete");

  /* Respond with an empty chunk to signal HTTP response completion */
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

// ----- upload_handler() -----

static esp_err_t 
upload_handler(httpd_req_t * req)
{
  LOG_D("upload_handler(%s)", req->uri);

  FILE *fd = NULL;
  struct stat file_stat;

  std::string filepath;
  /* Skip leading "/upload" from URI to get filename */
  /* Note sizeof() counts NULL termination hence the -1 */
  std::string filename = get_path_from_uri(filepath, 
                                           ((FileServerData *)req->user_ctx)->base_path,
                                           req->uri + sizeof("/upload") - 1);
  if (filename[filename.size() - 1] == '/') {
    LOG_E("Invalid filename : %s", filename.c_str());
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
    return ESP_FAIL;
  }

  if (stat(filepath.c_str(), &file_stat) == 0) {
    LOG_E("File already exists : %s", filepath.c_str());
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
    return ESP_FAIL;
  }

  if (req->content_len > MAX_FILE_SIZE) {
    LOG_E("File too large : %d bytes", req->content_len);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                        "File size must be less than "
                        MAX_FILE_SIZE_STR "!");
    return ESP_FAIL;
  }

  fd = fopen(filepath.c_str(), "w");
  if (!fd) {
    LOG_E("Failed to create file : %s", filepath.c_str());
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
    return ESP_FAIL;
  }

  LOG_I("Receiving file : %s...", filename.c_str());

  /* Retrieve the pointer to scratch buffer for temporary storage */
  char *buf = ((FileServerData *) req->user_ctx)->scratch;
  int received;

  /* Content length of the request gives
    * the size of the file being uploaded */
  int remaining = req->content_len;

  while (remaining > 0) {

    LOG_I("Remaining size : %d", remaining);
    if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
      if (received == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;
      }

      fclose(fd);
      unlink(filepath.c_str());

      LOG_E("File reception failed!");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
      return ESP_FAIL;
    }

    /* Write buffer content to file on storage */
    if (received && (received != fwrite(buf, 1, received, fd))) {
      /* Couldn't write everything to file!
        * Storage may be full? */
      fclose(fd);
      unlink(filepath.c_str());

      LOG_E("File write failed!");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
      return ESP_FAIL;
    }

    remaining -= received;
  }
  fclose(fd);
  LOG_I("File reception complete");

  /* Redirect onto root to see the updated file list */
  httpd_resp_set_status(req, "303 See Other");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_sendstr(req, "File uploaded successfully");
  return ESP_OK;
}

// ----- delete_handler() -----

static esp_err_t 
delete_handler(httpd_req_t * req)
{
  LOG_D("delete_handler(%s)", req->uri);

  struct stat file_stat;

  std::string filepath;
  /* Skip leading "/delete" from URI to get filename */
  /* Note sizeof() counts NULL termination hence the -1 */
  std::string filename = get_path_from_uri(filepath, 
                                           ((FileServerData *)req->user_ctx)->base_path,
                                           req->uri  + sizeof("/delete") - 1);
  if (filename[filename.size() - 1] == '/') {
    LOG_E("Invalid filename : %s", filename.c_str());
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
    return ESP_FAIL;
  }

  if (stat(filepath.c_str(), &file_stat) == -1) {
    LOG_E("File does not exist : %s", filepath.c_str());
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
    return ESP_FAIL;
  }

  LOG_I("Deleting file : %s", filepath.c_str());
  unlink(filepath.c_str());

  int pos = filepath.size() - 5;
  if (filepath.substr(pos).compare(".epub") == 0) {
    filepath.replace(pos, 5, ".pars");

    if (stat(filepath.c_str(), &file_stat) != -1) {
      LOG_I("Deleting file : %s", filepath.c_str());
      unlink(filepath.c_str());
    }

    filepath.replace(pos, 5, ".locs");

    if (stat(filepath.c_str(), &file_stat) != -1) {
      LOG_I("Deleting file : %s", filepath.c_str());
      unlink(filepath.c_str());
    }

    filepath.replace(pos, 5, ".toc");

    if (stat(filepath.c_str(), &file_stat) != -1) {
      LOG_I("Deleting file : %s", filepath.c_str());
      unlink(filepath.c_str());
    }
  }

  /* Redirect onto root to see the updated file list */
  httpd_resp_set_status(req, "303 See Other");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_sendstr(req, "File deleted successfully");
  return ESP_OK;
}

// ----- http_server_start() -----

static esp_err_t 
http_server_start()
{
  if (server_data != nullptr) {
    LOG_E("File server already started");
    return ESP_ERR_INVALID_STATE;
  }

  /* Allocate memory for server data */
  server_data = (FileServerData *) calloc(1, sizeof(FileServerData));
  if (server_data == nullptr) {
    LOG_E("Failed to allocate memory for server data");
    return ESP_ERR_NO_MEM;
  }
  strcpy(server_data->base_path, "/sdcard/books");

  httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();

  httpd_config.max_open_sockets = 4;

  int32_t port = 80;
  config.get(Config::Ident::PORT, &port);
  httpd_config.uri_match_fn = httpd_uri_match_wildcard;
  httpd_config.server_port = static_cast<uint16_t>(port);

  LOG_I("Starting HTTP Server on port %" PRIi32 "...", port);
  esp_err_t res = httpd_start(&server, &httpd_config);
  if (res != ESP_OK) {
    LOG_E("Failed to start file server (%s)!", esp_err_to_name(res));
    return ESP_FAIL;
  }

  httpd_uri_t file_download = {
    .uri       = "/*",  // Match all URIs of type /path/to/file
    .method    = HTTP_GET,
    .handler   = download_handler,
    .user_ctx  = server_data 
  };

  httpd_register_uri_handler(server, &file_download);

  httpd_uri_t file_upload = {
    .uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
    .method    = HTTP_POST,
    .handler   = upload_handler,
    .user_ctx  = server_data 
  };

  httpd_register_uri_handler(server, &file_upload  );

  httpd_uri_t file_delete = {
    .uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
    .method    = HTTP_POST,
    .handler   = delete_handler,
    .user_ctx  = server_data
  };

  httpd_register_uri_handler(server, &file_delete  );

  return ESP_OK;
}

// ----- http_server_stop() -----

static void
http_server_stop()
{
  if (server_data != nullptr) {
    httpd_stop(server);
    free(server_data);
    server_data = nullptr;
  }
}

bool
start_web_server(WebServerMode server_mode)
{
  page_locs.abort_threads();
  epub.close_file();

  msg_viewer.show(MsgViewer::MsgType::WIFI, false, true, 
    "Web Server Starting", 
    "The Web server is now establishing a connection with the WiFi router. Please wait.");

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    #define MSG "Tap the screen"
  #else
    #define MSG "Press a key"
  #endif

  if (((server_mode == WebServerMode::STA) && wifi.start_sta()) || 
      ((server_mode == WebServerMode::AP ) && wifi.start_ap())) {
    if (http_server_start() == ESP_OK) {
      esp_ip4_addr_t ip = wifi.get_ip_address();
      msg_viewer.show(MsgViewer::MsgType::WIFI, true, true, 
        "Web Server", 
        "The Web server is now running at ip " IPSTR ". To stop it, please " MSG ".", IP2STR(&ip));
    }
    else {
      msg_viewer.show(MsgViewer::MsgType::ALERT, true, true, 
        "Web Server Failed", 
        "The Web server was not able to start. Correct the situation and try again.");
      wifi.stop();
    }
  }
  else {
    msg_viewer.show(MsgViewer::MsgType::ALERT, true, true, 
      "WiFi Startup Failed", 
      "WiFi was not able to start the connection. Correct the situation and try again.");
    wifi.stop();
  }

  return true;
}

void
stop_web_server()
{
  http_server_stop();
  wifi.stop();
}

#endif