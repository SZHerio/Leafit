function Component()
{
}

Component.prototype.createOperations = function()
{
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
    }

    component.createOperations();
}
