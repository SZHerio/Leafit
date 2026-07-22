function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (installer.value("os") === "win") {
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/SZHBooks.exe",
            "@StartMenuDir@/SZHBooks.lnk",
            "workingDirectory=@TargetDir@"
        );
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/SZHBooks.exe",
            "@DesktopDir@/SZHBooks.lnk",
            "workingDirectory=@TargetDir@"
        );

        var executable = '"@TargetDir@/SZHBooks.exe"';
        var command = executable + ' "%1"';
        var icon = "@TargetDir@/SZHBooks.exe,0";
        var fileTypes = [
            ["txt", "Text book", "text/plain"],
            ["fb2", "FictionBook document", "application/x-fictionbook+xml"],
            ["epub", "EPUB book", "application/epub+zip"],
            ["pdf", "PDF document", "application/pdf"],
            ["html", "HTML document", "text/html"],
            ["htm", "HTML document", "text/html"],
            ["md", "Markdown document", "text/markdown"],
            ["markdown", "Markdown document", "text/markdown"],
            ["docx", "Word document", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"]
        ];
        for (var index = 0; index < fileTypes.length; ++index) {
            var fileType = fileTypes[index];
            component.addOperation(
                "RegisterFileType",
                fileType[0],
                command,
                fileType[1] + " - SZHBooks",
                fileType[2],
                icon,
                "ProgId=SZHBooks." + fileType[0]
            );
        }
    }
}
