const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const root_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "mousetester",
        .root_module = root_module,
    });

    exe.addCSourceFile(.{
        .file = b.path("src/main.c"),
        .flags = &.{"-std=c99", "-Wall", "-Wextra"},
    });
    exe.addCSourceFile(.{
        .file = b.path("src/mouse_log.c"),
        .flags = &.{"-std=c99", "-Wall", "-Wextra"},
    });
    exe.addCSourceFile(.{
        .file = b.path("src/plot.c"),
        .flags = &.{"-std=c99", "-Wall", "-Wextra"},
    });
    exe.addCSourceFile(.{
        .file = b.path("src/statistics.c"),
        .flags = &.{"-std=c99", "-Wall", "-Wextra"},
    });
    exe.addCSourceFile(.{
        .file = b.path("src/gui.c"),
        .flags = &.{"-std=c99", "-Wall", "-Wextra"},
    });

    exe.linkLibC();
    
    if (target.result.os.tag == .windows) {
        exe.linkSystemLibrary("user32");
        exe.linkSystemLibrary("gdi32");
        exe.linkSystemLibrary("comdlg32");
    }

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}