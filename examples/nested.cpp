// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#define CLI11_ENABLE_EXTRA_VALIDATORS 1
import std;
import cli11;

int main(int argc, char **argv)
{

    cli::app_t app("Vision Application");
    app.set_help_all_flag("--help-all", "Expand all help");
    app.add_flag("--version", "Get version");

    cli::app_t *cameraApp = app.add_subcommand("camera", "Configure the app camera");
    cameraApp->require_subcommand(0, 1); // 0 (default) or 1 camera

    std::string mvcamera_config_file = "mvcamera_config.json";
    cli::app_t *mvcameraApp = cameraApp->add_subcommand("mvcamera", "MatrixVision Camera Configuration");
    mvcameraApp->add_option("-c,--config", mvcamera_config_file, "Config filename")
        ->capture_default_str()
        ->check(cli::existing_file);

    std::string mock_camera_path;
    cli::app_t *mockcameraApp = cameraApp->add_subcommand("mock", "Mock Camera Configuration");
    mockcameraApp->add_option("-p,--path", mock_camera_path, "Path")->required()->check(cli::existing_path);

    try
    {
        app.parse(argc, argv);
    }
    catch (const cli::parse_error_t &e)
    {
        return app.exit(e);
    }
}
