/* Copyright (c) 2026 Tunuino. */
package io.tunuino.maintenance;

/** Source identity embedded in the shipped JAR manifest, independent of its folder. */
public final class BuildInfo {
    private BuildInfo() {}
    public static String revision() {
        String value = BuildInfo.class.getPackage().getImplementationVersion();
        return value != null && value.matches("[0-9a-fA-F]{40}") ? value : "development";
    }
    public static String label() {
        String revision = revision();
        return "build " + (revision.equals("development") ? revision : revision.substring(0, 7));
    }
}
