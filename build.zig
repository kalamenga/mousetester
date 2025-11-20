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

    const c_flags = &.{ "-std=c99", "-Wall", "-Wextra", "-O3", "-flto", "-ffast-math" };

    exe.addCSourceFile(.{
        .file = b.path("src/main.c"),
        .flags = c_flags,
    });
    exe.addCSourceFile(.{
        .file = b.path("src/mouse_log.c"),
        .flags = c_flags,
    });
    exe.addCSourceFile(.{
        .file = b.path("src/plot.c"),
        .flags = c_flags,
    });
    exe.addCSourceFile(.{
        .file = b.path("src/statistics.c"),
        .flags = c_flags,
    });
    exe.addCSourceFile(.{
        .file = b.path("src/gui.c"),
        .flags = c_flags,
    });
    exe.addCSourceFile(.{
        .file = b.path("src/wplot.c"),
        .flags = c_flags,
    });

    exe.linkLibC();

    exe.want_lto = true;

    exe.root_module.strip = (optimize != .Debug);

    if (target.result.os.tag == .windows) {
        exe.linkSystemLibrary("user32");
        exe.linkSystemLibrary("gdi32");
        exe.linkSystemLibrary("comdlg32");

        exe.subsystem = .Windows;

        exe.addWin32ResourceFile(.{
            .file = b.path("assets/icon.rc"),
        });
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
