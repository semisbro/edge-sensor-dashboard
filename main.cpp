#include "crow.h"

#include <cstdlib>
#include <string>

namespace {

int read_port() {
    if (const char* port = std::getenv("PORT")) {
        return std::stoi(port);
    }

    return 18080;
}

std::string openapi_document() {
    return R"({
  "openapi": "3.0.3",
  "info": {
    "title": "Crow Example API",
    "version": "1.0.0",
    "description": "A minimal Crow service with a static OpenAPI document."
  },
  "servers": [
    {
      "url": "http://localhost:18080"
    }
  ],
  "paths": {
    "/": {
      "get": {
        "summary": "Service metadata",
        "responses": {
          "200": {
            "description": "Basic service information"
          }
        }
      }
    },
    "/api/hello/{name}": {
      "get": {
        "summary": "Returns a greeting",
        "parameters": [
          {
            "name": "name",
            "in": "path",
            "required": true,
            "schema": {
              "type": "string"
            }
          }
        ],
        "responses": {
          "200": {
            "description": "Greeting payload"
          }
        }
      }
    },
    "/openapi.json": {
      "get": {
        "summary": "OpenAPI document",
        "responses": {
          "200": {
            "description": "The OpenAPI 3.0 spec for this service"
          }
        }
      }
    }
  }
})";
}

}  // namespace

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([] {
        crow::json::wvalue response;
        response["app"] = "portfolio_cpp";
        response["framework"] = "Crow";
        response["docs"] = "/openapi.json";
        response["hello_example"] = "/api/hello/world";
        return response;
    });

    CROW_ROUTE(app, "/api/hello/<string>")([](const std::string& name) {
        crow::json::wvalue response;
        response["message"] = "Hello, " + name + "!";
        response["framework"] = "Crow";
        return response;
    });

    CROW_ROUTE(app, "/openapi.json")([] {
        crow::response response(openapi_document());
        response.code = 200;
        response.set_header("Content-Type", "application/json");
        return response;
    });

    const int port = read_port();
    app.port(static_cast<std::uint16_t>(port)).multithreaded().run();
}
