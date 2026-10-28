/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.calibration.*;
import io.tunuino.datalog.LiveDataMonitor;
import io.tunuino.protocol.*;
import java.time.Duration;
import java.util.Arrays;
import java.util.function.Consumer;
import javafx.scene.control.*;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;

/** Actual USB acquisition, bounded learning and explicit verified persistence. */
final class AutoTunePane extends VBox {
    private final ConfigurationPane.Access access;
    private final Consumer<Boolean> onLearning;
    private final Consumer<CalibrationMap> onSaved;
    private final Button read=new Button("Ler base VE da ECU"),start=new Button("Iniciar recolha"),stop=new Button("Terminar recolha"),review=new Button("Rever proposta"),save=new Button("Guardar proposta na ECU");
    private final ComboBox<String> strength=new ComboBox<>();
    private final TextField target=new TextField("1.000");
    private final Label status=new Label("Lê primeiro a tabela VE e a configuração da ECU."),summary=new Label("Sem propostas.");
    private final TableView<VeAutotune.Cell> cells=new TableView<>();
    private DeviceIdentity identity;
    private VeAutotuneClient.Base base;
    private VeAutotune learner;
    private boolean connected,blocked,monitoring,armed,working,learning;

    AutoTunePane(Stage owner,ConfigurationPane.Access access,Consumer<Boolean> onLearning,Consumer<CalibrationMap> onSaved) {
        this.access=access;this.onLearning=onLearning;this.onSaved=onSaved;setSpacing(12);getStyleClass().add("autotune-pane");setPadding(new javafx.geometry.Insets(0,20,20,20));
        read.setId("autotune-read");start.setId("autotune-start");stop.setId("autotune-stop");save.setId("autotune-save");review.setId("autotune-review");status.setId("autotune-status");
        start.getStyleClass().add("primary");save.getStyleClass().add("primary");status.setWrapText(true);summary.setWrapText(true);
        strength.getItems().addAll("Suave · máximo ±2%","Normal · máximo ±5%");strength.getSelectionModel().select(0);strength.setId("autotune-strength");
        target.setPrefColumnCount(6);target.setId("autotune-target");
        String[] labels={"MAP (kPa)","RPM","Amostras","VE atual (%)","VE proposta (%)","Alteração (%)"};
        for(int i=0;i<labels.length;i++){
            final int at=i;TableColumn<VeAutotune.Cell,String> column=new TableColumn<>(labels[i]);column.setPrefWidth(i==0?105:125);
            column.setCellValueFactory(v->{var c=v.getValue();return new javafx.beans.property.ReadOnlyStringWrapper(switch(at){
                case 0->String.format(java.util.Locale.ROOT,"%.2f",base.map().loadKpaAt(c.row()));
                case 1->Integer.toString(base.map().rpmAt(c.column()));case 2->Integer.toString(c.samples());
                case 3->String.format(java.util.Locale.ROOT,"%.2f",c.before());case 4->String.format(java.util.Locale.ROOT,"%.2f",c.after());
                default->String.format(java.util.Locale.ROOT,"%+.2f",c.changePercent());});});cells.getColumns().add(column);
        }
        cells.setPlaceholder(new Label("As células aparecem após carga estável e amostras suficientes."));cells.setPrefHeight(340);
        Label note=new Label("Para guardar: termina a recolha, pára o motor/saídas e a monitorização. A base anterior é guardada em ficheiro e a gravação confirmada por releitura.");note.setWrapText(true);
        getChildren().addAll(ConsoleShell.card("AUTOTUNE VE · DADOS REAIS",
            "Aprende propostas com RPM, MAP, TPS, CLT e lambda wideband da ECU. Não arma saídas nem altera a tabela durante a recolha. O alvo segue a curva lambda ativa; sem curva, usa o valor abaixo.",
            ConsoleShell.actions(read,start,stop),ConsoleShell.actions(new Label("Correção por sessão"),strength,new Label("Lambda sem curva ativa"),target),status),
            cells,summary,ConsoleShell.actions(review,save),
            note);
        read.setOnAction(e->readBase());
        start.setOnAction(e->{try{
            double wanted=Double.parseDouble(target.getText().trim().replace(',','.'));
            learner=new VeAutotune(base.map(),base.generation(),base.core().fuelCorrections(),wanted,strength.getSelectionModel().getSelectedIndex()==0?2:5);
            cells.getItems().clear();learning=true;onLearning.accept(true);status.setText("Recolha USB iniciada. Aguarda motor quente e carga estável.");controls();
        }catch(RuntimeException x){status.setText(x.getMessage());}});
        stop.setOnAction(e->finish("Recolha terminada. Revê as células propostas."));
        review.setOnAction(e->CalibrationViewer.show(owner,"Autotune · proposta VE local.tnmap",learner.proposal()));
        save.setOnAction(e->saveProposal());controls();
    }
    private void readBase(){
        working=true;controls();
        access.run((session,device)->VeAutotuneClient.read(session,device),value->{working=false;base=value;learner=null;cells.getItems().clear();status.setText("Base VE recebida da ECU. Inicia Tempo real no cabeçalho e depois Iniciar recolha.");summary.setText("Geração "+value.generation()+" · nenhuma célula alterada");controls();},error->{working=false;status.setText("Leitura da base: "+error.getMessage());controls();});
    }
    void observe(LiveDataMonitor.TuningFrame frame){
        if(!learning||learner==null)return;
        if(!Arrays.equals(frame.extras().profile(),base.core().engineProfile())){finish("O perfil mudou: lê uma nova base antes de recolher.");base=null;controls();return;}
        learner.accept(frame);status.setText(learner.reason());cells.getItems().setAll(learner.cells());
        summary.setText(learner.accepted()+" amostras aceites · "+learner.rejected()+" excluídas · "+cells.getItems().size()+" células propostas");
        if(learner.invalidated())finish(learner.reason());controls();
    }
    private void finish(String message){learning=false;onLearning.accept(false);status.setText(message);controls();}
    private void saveProposal(){
        var expected=base;CalibrationMap proposed=learner.proposal();working=true;controls();
        access.run((session,device)->{
            java.nio.file.Path backup=io.tunuino.maintenance.AppPaths.maps().resolve("autotune-base-"+java.util.UUID.randomUUID()+".tnmap");
            VeAutotuneClient.write(session,device,expected,proposed,backup);return backup;
        },backup->{working=false;onSaved.accept(proposed);base=null;learner=null;cells.getItems().clear();status.setText("Proposta guardada e verificada na ECU. Base anterior: "+backup);summary.setText("Lê uma nova base para a próxima sessão.");controls();},error->{working=false;status.setText("Gravação: "+error.getMessage());controls();});
    }
    void availability(DeviceIdentity device,boolean connected,boolean blocked,boolean monitoring,boolean armed){
        if(!connected||!java.util.Objects.equals(identity,device)){
            if(learning)finish("Ligação/ECU alterada. Recolha terminada.");base=null;learner=null;cells.getItems().clear();
        }
        identity=device;this.connected=connected&&device!=null;this.blocked=blocked;this.monitoring=monitoring;this.armed=armed;
        if(!monitoring&&learning)finish("Monitorização terminada. Revê a proposta antes de guardar.");controls();
    }
    private void controls(){
        boolean ready=connected&&!working;
        read.setDisable(!ready||blocked||monitoring||learning);
        start.setDisable(!ready||base==null||!monitoring||learning);
        stop.setDisable(!learning);strength.setDisable(learning||working);target.setDisable(learning||working);
        boolean proposal=learner!=null&&!learner.invalidated()&&base!=null&&!learner.cells().isEmpty();
        review.setDisable(!proposal||learning||working);save.setDisable(!ready||blocked||monitoring||armed||learning||!proposal);
    }
}
