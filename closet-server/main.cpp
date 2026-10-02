#include "httplib.h"
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

  // Register the route
  server.Post("/items", [] (const httplib::Request &req, httplib::Response &resp) {
    // Check the request is valid (photo and name present), else 400
    if (!req.form.has_file("photo") || req.form.get_field("name").empty()) {
      resp.status = 400;
      resp.set_content("Invalid Request", "text/plain");
      return; // Stop the lambda
    }
    const auto& photo = req.form.get_file("photo");
    const string name = req.form.get_field("name");

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

    // Respond with success and the saved filename
    resp.status = 200;
    resp.set_content(fileName, "text/plain");
    return;
});

  // Listen on port 8080
  server.listen("0.0.0.0", 8080);
}