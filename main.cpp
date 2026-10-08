#include "httplib.h"
#include "sqlite3.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <system_error>
#include <ctime> // for the timestamp to mark the photo
#include <string> // for converting epoch time into a string for filename
#include <vector>
#include <algorithm> // for find() function used in target search in vector
using namespace std;


int main(){
  httplib::Server server;

  // Check if uploads/ folder exists
  filesystem::create_directory("uploads/");


  server.Get("/hi", [](const httplib::Request&, httplib::Response &resp) {
    // Send back the text
    resp.set_content("hi closet!", "text/plain");
  });

  /* Open closet.db with sqlite3_open
with a parameter of type sqlite3* pointer that represents the open database)
sqlite3_open returns a status number
Check that it equals SQLITE_OK, and print sqlite3_errmsg(...) and exit if not.
*/

sqlite3 *closetHandler; // closet 95 handler variable that points to the DB object
int sqliteOpen = sqlite3_open("closet.db", &closetHandler); // passing in the address of the pointer variable
// sqlite3_open returns a status code
if (sqliteOpen != SQLITE_OK) {
  cerr << sqlite3_errmsg(closetHandler) << endl; // exit cannot be before otherwise it will never print
  sqlite3_close(closetHandler);
  exit(sqliteOpen);
}

string sql_create = "CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY NOT NULL, name TEXT NOT NULL, category TEXT NOT NULL, occasion TEXT NOT NULL, filename TEXT NOT NULL, created_at INTEGER NOT NULL)";

int sqliteExec = sqlite3_exec(closetHandler, sql_create.c_str(), nullptr, nullptr, nullptr);
if (sqliteExec != SQLITE_OK) {
  cerr << sqlite3_errmsg(closetHandler) << endl;
  sqlite3_close(closetHandler); // close DB
  exit(sqliteExec); // give status code
}


  // Register the route
  server.Post("/items", [closetHandler] (const httplib::Request &req, httplib::Response &resp) {
    // Check the request is valid (photo and name present), else 400
      // NOTE: we use get_field and not has_field because has_field will return true even if the field is empty
    if (!req.form.has_file("photo") || req.form.get_field("name").empty() || req.form.get_field("category").empty() || req.form.get_field("occasion").empty()) {
      resp.status = 400;
      resp.set_content("Invalid Request", "text/plain");
      return; // Stop the lambda
    }
    const auto& photo = req.form.get_file("photo");
    const string name = req.form.get_field("name");
    const string category = req.form.get_field("category");
    const string occasion = req.form.get_field("occasion");


    // Pick your own filename (counter or timestamp)
    time_t timestamp;
    time(&timestamp);
    string fileName = to_string(timestamp);

    // Check extension
    auto extension = (filesystem::path(photo.filename).extension()).string();
    vector<string> allowedExtensions = {".jpg",".png", ".jpeg", ".webp"};
    auto it = find(allowedExtensions.begin(), allowedExtensions.end(), extension);
    if (it == allowedExtensions.end()) {
      resp.status = 400;
      resp.set_content("File Extension Invalid", "text/plain");
      return;
    }
    fileName += extension;
    // Write the photo's bytes to uploads/<filename> in binary mode
    ofstream ofs("uploads/" + fileName, ios::binary); // how we are moving the bytes of the photo upload from memory onto the disk
    if (!ofs) {
      resp.status = 500;
      resp.set_content("Failure to Open File", "text/plain");
      return;
    }
    ofs << photo.content;
    ofs.close();

    // Insert with ? placeholders
string sql_insert = "INSERT INTO items (name, category, occasion, filename, created_at) VALUES (?, ?, ?, ?, ?)";

// Compiling An SQL Statement using sqlite3_prepare_v2 - https://www.sqlite.org/c3ref/prepare.html
sqlite3_stmt *out;
auto prepStatus = sqlite3_prepare_v2(closetHandler, sql_insert.c_str(), -1, &out, nullptr); // -1 param is for length of 2nd param, -1 says to read till end
if (prepStatus != SQLITE_OK) {
  cerr << sqlite3_errmsg(closetHandler) << endl;
  resp.status = 500;
  resp.set_content("Compilation Failed", "text/plain");
  filesystem::remove("uploads/" + fileName);
  return;
}
// Bind values to prepared statements - https://www.sqlite.org/c3ref/bind_blob.html
// int sqlite3_bind_text(sqlite3_stmt*,int,const char*,int,void(*)(void*));
sqlite3_bind_text(out, 1, name.c_str(), -1, SQLITE_STATIC);
sqlite3_bind_text(out, 2, category.c_str(), -1, SQLITE_STATIC);
sqlite3_bind_text(out, 3, occasion.c_str(), -1, SQLITE_STATIC);
sqlite3_bind_text(out, 4, fileName.c_str(), -1, SQLITE_STATIC);

// int sqlite3_bind_int64(sqlite3_stmt*, int, sqlite3_int64);
sqlite3_bind_int64(out, 5, timestamp);

// Step - Evaluate an SQL statement/run it
// int sqlite3_step(sqlite3_stmt*);
auto stepStatus = sqlite3_step(out);
if (stepStatus != SQLITE_DONE) {
  cerr << sqlite3_errmsg(closetHandler) << endl;
  resp.status = 500;
  resp.set_content("Evaluation Failed", "text/plain");
  filesystem::remove("uploads/" + fileName);

  sqlite3_finalize(out);
  return;
}
// Respond with success and the saved filename
resp.status = 200;
resp.set_content(fileName, "text/plain");

// Finalize to release/delete prepared stmt obj
// int sqlite3_finalize(sqlite3_stmt *pStmt);
sqlite3_finalize(out);

return;
});


auto mount = server.set_mount_point("/", "./public");

/*

(add something meaningful when the page searched for doesn't exist)
if (!mount) {

}

*/

  // Listen on port 8080
  server.listen("0.0.0.0", 8080);
  sqlite3_close(closetHandler);
}