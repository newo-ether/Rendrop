# Rendrop

Rendrop is a lightweight, cross-platform render management tool designed to streamline Blender rendering workflows. It provides a centralized interface for managing render queues, monitoring progress, and accessing render status remotely via a built-in API.

## Key Features

*   **Automated Project Parsing**: Automatically extracts resolution, frame ranges, and render engine settings directly from `.blend` files.
*   **Queue Management**: Easily add projects via drag-and-drop, reorder tasks, and customize frame ranges for individual render jobs.
*   **Version Manager**: Manage multiple Blender installations and select specific versions for different projects.
*   **Real-time Monitoring**: Track rendering progress through detailed progress bars and live console output integration.
*   **Built-in HTTP API**: Remote monitoring capabilities allowing you to check project status and preview rendered frames from any device on the network.
*   **Modern UI**: High-performance interface featuring custom widgets, smooth animations, and internationalization support.

## Technical Stack

*   **Framework**: Qt 6 (Widgets, Network, Gui)
*   **Communication**: `cpp-httplib` for the embedded web server.
*   **Data Handling**: `nlohmann/json` for API responses and configuration.
*   **Integration**: Direct interaction with Blender CLI via background Python script injection.

## HTTP API

Rendrop hosts a REST API on port `28528` for remote integration:

*   `GET /projects`: Retrieve a list of all projects and their current status (frame progress, resolution, etc.).
*   `GET /projects/{id}`: Get detailed information for a specific project.
*   `GET /frame?id={id}&frame={n}&thumb={0|1}`: Fetch a specific rendered frame or a 128x128 thumbnail.

## Getting Started

### Prerequisites
*   Qt 6.x
*   Blender (installed and added to Rendrop's version manager)
*   MSVC 2022 (for Windows builds)

### Build Instructions
1. Open `Rendrop.pro` in Qt Creator.
2. Ensure the submodules in `thirdparty/` are initialized.
3. Build and run the project in Release mode for optimal performance.

## License

This project is licensed under the terms specified in the repository.
