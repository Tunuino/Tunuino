/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;
import io.tunuino.calibration.EngineProject;
import io.tunuino.protocol.*;
import java.nio.file.*;
import java.time.Duration;
import java.util.function.Consumer;
import javafx.concurrent.Task;
import javafx.scene.control.*;
import javafx.scene.layout.VBox;
import javafx.stage.FileChooser;
import javafx.stage.Stage;

/** Complete ECU calibration backups and explicitly confirmed atomic restoration. */
final class EngineProjectPane extends VBox implements AutoCloseable {
    private final ConfigurationPane.Access access;
    private final Consumer<EngineProjectClient.Snapshot> onRestored;
    private final Stage owner;
    private final Button read=new Button("Ler projeto completo da ECU"),open=new Button("Abrir projeto"),copy=new Button("Guardar cópia"),restore=new Button("Restaurar projeto na ECU");
    private final Label source=new Label("Sem projeto aberto."),details=new Label("Motor, sensores, rodas, mapas VE/ignição, ralenti, turbo e proteções num único ficheiro .tnproj."),result=new Label();
    private final java.util.concurrent.ExecutorService files=java.util.concurrent.Executors.newSingleThreadExecutor(r->{Thread t=new Thread(r,"tunuino-project-file");t.setDaemon(true);return t;});
    private DeviceIdentity identity;
    private EngineProject project;
    private EngineProjectClient.Snapshot snapshot;
    private boolean available,armed,busy;
    EngineProjectPane(Stage owner,ConfigurationPane.Access access,Consumer<EngineProjectClient.Snapshot> onRestored){
        this.owner=owner;this.access=access;this.onRestored=onRestored;setSpacing(12);setPadding(new javafx.geometry.Insets(0,20,20,20));setId("engine-project-pane");getStyleClass().add("engine-project-pane");
        read.setId("project-read");open.setId("project-open");copy.setId("project-copy");restore.setId("project-restore");source.setId("project-source");result.setId("project-result");
        restore.getStyleClass().add("primary");source.setWrapText(true);details.setWrapText(true);result.setWrapText(true);
        getChildren().addAll(ConsoleShell.card("PROJETO COMPLETO DA ECU","Abrir e guardar ficheiros não altera a centralina. Restaurar exige saídas desarmadas, uma leitura atual da mesma ECU e confirmação do projeto.",ConsoleShell.actions(read,open,copy),source,details),ConsoleShell.actions(restore),result);
        read.setOnAction(e->{busy=true;controls();access.run((s,d)->EngineProjectClient.read(s,d,Duration.ofSeconds(30)),value->{busy=false;snapshot=value;project=value.project();source.setText("Projeto completo lido da ECU · geração "+value.validation().generation());details.setText(project.description());result.setText("Usa Guardar cópia para criar um ficheiro. Para restaurar outro projeto, abre-o mantendo esta leitura como referência.");controls();},this::failed);});
        open.setOnAction(e->{var chooser=chooser("Abrir projeto completo");var file=chooser.showOpenDialog(owner);if(file==null)return;
            fileTask(()->EngineProject.read(file.toPath()),value->{project=value;source.setText("Ficheiro: "+file.getName());details.setText(project.description());result.setText(snapshot==null?"Lê primeiro o estado atual da ECU antes de restaurar.":"Projeto aberto apenas localmente. Revê os valores e usa Restaurar para enviar.");});});
        copy.setOnAction(e->{if(project==null)return;var chooser=chooser("Guardar nova cópia do projeto");chooser.setInitialFileName("Tunuino-Projeto.tnproj");var file=chooser.showSaveDialog(owner);if(file==null)return;
            Path path=file.toPath();if(!file.getName().toLowerCase(java.util.Locale.ROOT).endsWith(".tnproj"))path=path.resolveSibling(file.getName()+".tnproj");
            final Path destination=path;EngineProject value=project;fileTask(()->{value.writeNew(destination);return destination;},saved->result.setText("Projeto completo guardado em "+saved));});
        restore.setOnAction(e->{if(project==null||snapshot==null)return;
            Alert confirm=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,"Restaurar substitui motor, sensores, tabelas e auxiliares pelo projeto aberto.\n\n"+project.description()+"\n\nSerá criada uma cópia completa do estado atual. Guardar não arma saídas nesta sessão.",ButtonType.OK,ButtonType.CANCEL);
            confirm.initOwner(owner);confirm.setHeaderText("Restaurar as quatro páginas da configuração?");
            if(confirm.showAndWait().orElse(ButtonType.CANCEL)!=ButtonType.OK)return;
            var expected=snapshot;var desired=project;busy=true;controls();
            Path backup=io.tunuino.maintenance.AppPaths.configurations().resolve("projeto-anterior-"+java.util.UUID.randomUUID()+".tnproj");
            access.run((s,d)->EngineProjectClient.apply(s,d,expected,desired,backup,Duration.ofSeconds(60)),saved->{busy=false;snapshot=saved;project=saved.project();source.setText("Projeto guardado e verificado · geração "+saved.validation().generation());details.setText(project.description());result.setText("Restauro confirmado por releitura completa. Cópia anterior: "+backup+". Lê o perfil em Motor e saídas antes de armar.");onRestored.accept(saved);controls();},this::failed);
        });controls();
    }
    private FileChooser chooser(String title){var c=new FileChooser();c.setTitle(title);c.getExtensionFilters().add(new FileChooser.ExtensionFilter("Projeto completo Tunuino","*.tnproj"));ConsoleFiles.in(c,io.tunuino.maintenance.AppPaths.configurations());return c;}
    @FunctionalInterface private interface FileWork<T>{T run()throws Exception;}
    private <T>void fileTask(FileWork<T> work,Consumer<T> success){busy=true;controls();Task<T> task=new Task<>(){@Override protected T call()throws Exception{return work.run();}};
        task.setOnSucceeded(e->{busy=false;success.accept(task.getValue());controls();});task.setOnFailed(e->failed(task.getException()));files.execute(task);}
    private void failed(Throwable error){busy=false;result.setText("Projeto: "+error.getMessage());controls();}
    void availability(DeviceIdentity device,boolean connected,boolean blocked,boolean armed){
        if(!connected||!java.util.Objects.equals(identity,device))snapshot=null;
        identity=connected?device:null;available=connected&&device!=null&&!blocked;this.armed=armed;controls();
    }
    boolean isBusy(){return busy;}
    private void controls(){read.setDisable(!available||busy);open.setDisable(busy);copy.setDisable(busy||project==null);restore.setDisable(!available||busy||armed||project==null||snapshot==null);}
    @Override public void close(){files.shutdown();}
}
