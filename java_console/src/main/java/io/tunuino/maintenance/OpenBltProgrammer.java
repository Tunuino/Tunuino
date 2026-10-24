/* Copyright (c) 2026 Tunuino. */
package io.tunuino.maintenance;

import java.io.IOException;
import java.nio.file.Path;
import java.time.Duration;
import java.util.List;
import java.util.function.Consumer;

/** Manual OpenBLT mode delegates XCP to the official BootCommander host tool. */
public final class OpenBltProgrammer {
    private OpenBltProgrammer() {}
    public static List<String> command(Path tool, String port, Path srec) throws IOException {
        if (!port.matches("(?i)COM[1-9][0-9]*")) throw new IOException("Porta COM inválida");
        return List.of(tool.toString(), "-s=xcp", "-t=xcp_rs232", "-d=" + port, "-b=115200", srec.toString());
    }
    public static void flash(Path tool, String port, FirmwareImage image, Consumer<String> log) throws IOException {
        if (!image.mode().equals("openblt") || image.replacesBootloader())
            throw new IOException("É necessária uma imagem .srec própria para OpenBLT");
        try (FirmwareImage.Staged staged = image.stage()) {
            CommandRunner.Result result = CommandRunner.run(command(tool, port, staged.path()), tool.getParent(),
                Duration.ofMinutes(5), log);
            if (result.exitCode() != 0) throw new IOException("OpenBLT terminou com erro " + result.exitCode() + ". Consulta o registo.");
        }
        log.accept("\nBootCommander terminou sem erro. Volta a ligar e identifica a ECU para confirmar o arranque.\n");
    }
}
