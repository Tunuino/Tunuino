/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.calibration.CalibrationMap;
import io.tunuino.datalog.LogSample;
import io.tunuino.protocol.DeviceIdentity;
import java.util.EnumMap;
import java.util.Locale;
import java.util.Optional;
import java.util.function.Consumer;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.canvas.Canvas;
import javafx.scene.canvas.GraphicsContext;
import javafx.scene.control.Button;
import javafx.scene.control.CheckBox;
import javafx.scene.control.Label;
import javafx.scene.control.ToggleButton;
import javafx.scene.control.ToggleGroup;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.FlowPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.control.SplitPane;
import javafx.scene.layout.VBox;
import javafx.scene.paint.Color;
import javafx.scene.shape.ArcType;
import javafx.scene.text.Font;
import javafx.scene.text.TextAlignment;

/** Real-data-only 16×16 dashboard. A blank value means unavailable, never zero. */
final class DashboardPane extends VBox {
    private enum Channel { RPM, MAP, LAMBDA, COOLANT }
    private static final Color GRID=Color.web("#273c52");
    private static final Color WHITE=Color.web("#e8f2ff");
    private static final Color DIM=Color.web("#8da2ba");
    private final EnumMap<Channel,Gauge> gauges=new EnumMap<>(Channel.class);
    private final Label status=new Label("ECU desligada · aguarda ligação USB");
    private final Label source=new Label("Sem valores de sensores disponíveis");
    private final Label mapSource=new Label("Liga e identifica a ECU para ler as tabelas.");
    private final Label viewCaption=new Label("Mapas da ECU · a aguardar leitura real");
    private final Label connection=new Label("●  USB DESLIGADO");
    private final Canvas tableCanvas=new Canvas(840,315);
    private final Canvas surfaceCanvas=new Canvas(470,285);
    private final Canvas ignitionOverview=new Canvas(740,285);
    private final CanvasPane ignitionHost=new CanvasPane(ignitionOverview);
    private final CanvasPane surfaceHost=new CanvasPane(surfaceCanvas);
    private final CanvasPane tableHost=new CanvasPane(tableCanvas);
    private final ToggleButton veTab=new ToggleButton("VE 16×16");
    private final ToggleButton sparkTab=new ToggleButton("Ignição 16×16");
    private final Button readVe=new Button("Ler VE da ECU");
    private final Button readSpark=new Button("Ler ignição da ECU");
    private final Button editMap=new Button("Editar tabela");
    private final CheckBox show3d=new CheckBox("Vista 3D");
    private final CheckBox liveHighlight=new CheckBox("Realçar célula ativa");
    private final Label cellDetails=new Label("Seleciona uma célula do mapa para consultar o valor.");
    private final EnumMap<CalibrationMap.Kind,CalibrationMap> maps=new EnumMap<>(CalibrationMap.Kind.class);
    private final FlowPane gaugeRow=new FlowPane(10,8);
    private final FlowPane smallRow=new FlowPane(9,9);
    private Consumer<CalibrationMap.Kind> onRead=kind->{};
    private Consumer<CalibrationMap.Kind> onBase=kind->{};
    record MapSelection(CalibrationMap map,int row,int column) {}
    private Consumer<MapSelection> onEdit=selection->{};
    private DeviceIdentity mapOwner;
    private CalibrationMap.Kind selected=CalibrationMap.Kind.VE;
    private boolean online;
    private LogSample latestSample;
    private boolean readingAllowed;
    private String selectedPort="USB";
    private final Label portLabel=new Label("PORTA —");
    private final Label firmwareLabel=new Label("Firmware —");
    private final Gauge tpsGauge=new Gauge("TPS","%","#24df38",0,100);
    private final Gauge iatGauge=new Gauge("IAT","°C","#59b9ee",-20,120);
    private final java.util.Map<String,Label> smallValues=new java.util.HashMap<>();
    private final Label lambdaValue=new Label("— λ");
    private final java.util.Map<String,Label> outputValues=new java.util.HashMap<>();
    private final Label outputs=new Label("Estado das saídas por consultar");
    private final Label camPhase=new Label("Cames: por consultar");

    void onBaseMap(Consumer<CalibrationMap.Kind> action){onBase=java.util.Objects.requireNonNull(action);}

    DashboardPane() { this(false); }

    /** Maps-only cockpit reuses precisely the same real 16x16 rendering logic. */
    DashboardPane(boolean mapsOnly) {
        setId(mapsOnly?"dashboard-map-workspace":"dashboard");
        getStyleClass().add("ecu-dashboard");
        setSpacing(5);
        setPadding(new Insets(4,5,5,5));
        Label title=new Label("TUNUINO  /  PAINEL DA ECU");
        title.getStyleClass().add("dashboard-kicker");
        status.getStyleClass().add("dashboard-status");
        source.getStyleClass().add("dashboard-subtext");
        connection.getStyleClass().add("dashboard-connected");
        portLabel.getStyleClass().add("dashboard-subtext");
        firmwareLabel.getStyleClass().add("dashboard-subtext");
        VBox identity=new VBox(3,connection,new HBox(12,portLabel,firmwareLabel));
        identity.getStyleClass().add("dashboard-identity");
        HBox header=new HBox(12,new VBox(5,title,status,source),new javafx.scene.layout.Region(),identity);
        HBox.setHgrow(header.getChildren().get(1),Priority.ALWAYS);
        header.setAlignment(Pos.CENTER_LEFT);

        for(Channel channel:Channel.values())gauges.put(channel,new Gauge(channel));
        gaugeRow.getChildren().addAll(gauges.get(Channel.RPM),gauges.get(Channel.MAP),
            tpsGauge,gauges.get(Channel.COOLANT),iatGauge,gauges.get(Channel.LAMBDA));
        gaugeRow.widthProperty().addListener((property,before,width)->{
            int columns=width.doubleValue()>=760?6:3;
            double cardWidth=Math.floor((width.doubleValue()-10*(columns-1))/columns)-1;
            for(var child:gaugeRow.getChildren())((Gauge)child).setPrefWidth(Math.max(108,cardWidth));
        });
        gaugeRow.setPrefWrapLength(1240);
        smallRow.setPrefWrapLength(1240);
        smallRow.setHgap(4);smallRow.setVgap(4);
        for(String[] entry:new String[][]{
            {"BATERIA","— V"},{"PRESSÃO COMB.","— bar"},
            {"PRESSÃO DE ÓLEO","— bar"},{"TEMP. ÓLEO","— °C"},
            {"TEMP. COMBUSTÍVEL","— °C"},{"LAMBDA 1","— λ"},{"LAMBDA 2","— λ"},
            {"VELOCIDADE","— km/h"}}) {
            Label value=entry[0].equals("LAMBDA 1")?lambdaValue:new Label(entry[1]);smallValues.put(entry[0],value);value.getStyleClass().add("dashboard-small-value");
            Label name=new Label(entry[0]);name.getStyleClass().add("dashboard-small-title");
            VBox tile=new VBox(3,name,value);
            tile.getStyleClass().add("dashboard-small-card");
            tile.setPrefWidth(168);smallRow.getChildren().add(tile);
        }
        smallRow.widthProperty().addListener((property,before,width)->{
            int columns=width.doubleValue()>=760?8:4;
            double cardWidth=Math.floor((width.doubleValue()-4*(columns-1))/columns)-1;
            for(var child:smallRow.getChildren())((VBox)child).setPrefWidth(Math.max(110,cardWidth));
        });

        ToggleGroup tabs=new ToggleGroup();veTab.setToggleGroup(tabs);sparkTab.setToggleGroup(tabs);
        veTab.setSelected(true);
        veTab.setId("dashboard-tab-ve");sparkTab.setId("dashboard-tab-ignition");
        veTab.getStyleClass().add("dashboard-tab");sparkTab.getStyleClass().add("dashboard-tab");
        veTab.setOnAction(event->{selected=CalibrationMap.Kind.VE;redrawMaps();});
        sparkTab.setOnAction(event->{selected=CalibrationMap.Kind.IGNITION;redrawMaps();});
        show3d.setSelected(true);show3d.setId("dashboard-3d-toggle");
        show3d.setOnAction(event->{
            surfaceHost.setVisible(show3d.isSelected());
            surfaceHost.setManaged(show3d.isSelected());
        });
        liveHighlight.setId("dashboard-live-highlight");
        liveHighlight.setSelected(true);
        liveHighlight.setOnAction(event->drawHeatmap());
        readVe.setId("dashboard-read-ve");readSpark.setId("dashboard-read-ignition");
        readVe.getStyleClass().add("primary");
        Button base=new Button("Criar mapa base");base.setId("dashboard-base-map");base.setOnAction(event->onBase.accept(selected));
        readVe.setOnAction(event->onRead.accept(CalibrationMap.Kind.VE));
        readSpark.setOnAction(event->onRead.accept(CalibrationMap.Kind.IGNITION));
        editMap.setId("dashboard-edit-map");
        editMap.setOnAction(event->editSelected(0,0));
        FlowPane controls=new FlowPane(8,8,veTab,sparkTab,
            readVe,readSpark,editMap,base,liveHighlight,show3d);
        controls.getStyleClass().add("dashboard-table-actions");
        mapSource.getStyleClass().add("dashboard-subtext");mapSource.setWrapText(true);
        tableHost.getStyleClass().add("dashboard-table-host");
        tableHost.setMinHeight(150);tableHost.setPrefHeight(265);
        cellDetails.setWrapText(true);
        cellDetails.getStyleClass().add("dashboard-subtext");
        tableCanvas.setOnMouseMoved(event->{
            CalibrationMap map=maps.get(selected);
            if(map==null)return;
            double w=tableCanvas.getWidth(),h=tableCanvas.getHeight();
            if(w<=80||h<=46||event.getX()<72||event.getY()<38)return;
            double cw=(w-80)/16,ch=(h-46)/16;
            int col=(int)((event.getX()-72)/cw),row=(int)((event.getY()-38)/ch);
            if(col>=0&&col<16&&row>=0&&row<16)
                cellDetails.setText(String.format(Locale.ROOT,
                    "RPM %d · MAP %.1f kPa · %s %.3f %s",
                    map.rpmAt(col),map.loadKpaAt(row),selected,map.cell(row,col),
                    selected==CalibrationMap.Kind.VE?"%":"°"));
        });
        tableCanvas.setId("dashboard-map-canvas");
        tableCanvas.setOnMouseClicked(event->{
            if(event.getClickCount()!=2||event.getButton()!=javafx.scene.input.MouseButton.PRIMARY)return;
            double cw=(tableCanvas.getWidth()-80)/16,ch=(tableCanvas.getHeight()-46)/16;
            double x=event.getX()-72,y=event.getY()-38;
            if(cw<=0||ch<=0||x<0||y<0)return;
            int column=(int)(x/cw),row=(int)(y/ch);
            if(row<16&&column<16)editSelected(row,column);
        });
        tableCanvas.widthProperty().addListener((a,b,c)->drawHeatmap());
        tableCanvas.heightProperty().addListener((a,b,c)->drawHeatmap());
        VBox tablePanel=new VBox(4,controls,viewCaption,tableHost,cellDetails);
        tablePanel.getStyleClass().add("dashboard-card");
        tablePanel.setMinWidth(0);VBox.setVgrow(tableHost,Priority.ALWAYS);
        tablePanel.setPrefWidth(840);
        HBox.setHgrow(tablePanel,Priority.ALWAYS);

        Label note=new Label("Perfil e armação explícita em Motor e saídas.");
        note.setWrapText(true);note.getStyleClass().add("dashboard-subtext");
        outputs.setId("dashboard-engine-status");
        outputs.getStyleClass().add("dashboard-output-status");
        VBox signalRows=new VBox(8);
        for(String[] role:new String[][]{{"pump","Bomba"},{"relay","Relé principal"},{"fan","Ventoinha 1"},{"fan2","Ventoinha 2"},{"ac","Pedido A/C"},{"launch","Launch"},{"shift","Flat shift"},{"antilag","Anti-lag"}}){
            Label name=new Label(role[1]),value=new Label("—");value.setId("dashboard-output-"+role[0]);outputValues.put(role[0],value);value.setStyle("-fx-text-fill:#8b9aa7;-fx-font-size:10px;");
            name.setStyle("-fx-font-size:10px;");javafx.scene.layout.Region gap=new javafx.scene.layout.Region();HBox.setHgrow(gap,Priority.ALWAYS);signalRows.getChildren().add(new HBox(3,name,gap,value));
        }
        camPhase.setId("dashboard-cam-phase");camPhase.setWrapText(true);
        camPhase.setTooltip(new javafx.scene.control.Tooltip("Ângulo da aresta selecionada em graus de cambota desde o primeiro dente após a falha e a paridade configurada. Não é avanço VVT nem referência PMS. VVT ainda não comandado."));
        VBox side=new VBox(8,new Label("ESTADO DO MOTOR"),signalRows,outputs,camPhase,new Label("COMUNICAÇÃO"),connection,portLabel,firmwareLabel);
        side.getStyleClass().add("dashboard-card");side.setMinWidth(0);
        var sideScroll=ConsoleShell.scroll(side);sideScroll.setMinWidth(165);
        SplitPane center=new SplitPane(tablePanel,sideScroll);center.setId("dashboard-center-split");
        center.getStyleClass().add("dashboard-split");center.setDividerPositions(.80);
        center.setMinSize(0,300);tablePanel.setMinHeight(260);side.setMinWidth(165);
        center.setAccessibleHelp("Arrasta o separador para ajustar a largura da tabela e do estado do motor.");

        Label surfaceTitle=new Label("MAPA 3D  ·  VISUALIZAÇÃO DAS CÉLULAS");
        surfaceTitle.getStyleClass().add("dashboard-small-title");
        surfaceHost.getStyleClass().add("dashboard-table-host");
        surfaceHost.setMinHeight(120);surfaceHost.setPrefHeight(215);
        surfaceCanvas.widthProperty().addListener((a,b,c)->drawSurface());
        surfaceCanvas.heightProperty().addListener((a,b,c)->drawSurface());
        VBox surfacePanel=new VBox(8,surfaceTitle,surfaceHost);
        surfacePanel.getStyleClass().add("dashboard-card");
        VBox.setVgrow(surfaceHost,Priority.ALWAYS);
        VBox.setVgrow(surfacePanel,Priority.ALWAYS);
        surfacePanel.setMinWidth(0);
        ignitionHost.getStyleClass().add("dashboard-table-host");
        ignitionHost.setMinHeight(120);ignitionHost.setPrefHeight(215);
        ignitionOverview.widthProperty().addListener((a,b,c)->drawIgnition());
        ignitionOverview.heightProperty().addListener((a,b,c)->drawIgnition());
        Label secondTitle=new Label("TABELA DE IGNIÇÃO · 16×16");
        secondTitle.getStyleClass().add("dashboard-small-title");
        VBox ignitionPanel=new VBox(8,secondTitle,ignitionHost);
        ignitionPanel.getStyleClass().add("dashboard-card");
        ignitionPanel.setMinWidth(0);VBox.setVgrow(ignitionHost,Priority.ALWAYS);
        SplitPane lower=new SplitPane(ignitionPanel,surfacePanel);lower.setId("dashboard-lower-split");
        lower.getStyleClass().add("dashboard-split");lower.setDividerPositions(.63);lower.setMinSize(0,180);
        SplitPane tables=new SplitPane(center,lower);tables.setOrientation(javafx.geometry.Orientation.VERTICAL);
        tables.setId("dashboard-tables-split");tables.getStyleClass().add("dashboard-split");
        tables.setDividerPositions(.60);tables.setMinSize(0,490);tables.setPrefHeight(640);
        VBox.setVgrow(tables,Priority.ALWAYS);
        if(mapsOnly) {
            Label heading=new Label("TABELAS NATIVAS 16×16 · ECU");
            heading.getStyleClass().add("dashboard-kicker");
            getChildren().addAll(heading,tables);
        } else {
            getChildren().addAll(gaugeRow,smallRow,tables);
        }
        setConnection(null,false,false);
        redrawMaps();
    }

    void setReadAction(Consumer<CalibrationMap.Kind> action) {
        onRead=java.util.Objects.requireNonNull(action);
    }
    void showAuxiliary(io.tunuino.protocol.AuxControlClient.FeatureState value){
        tpsGauge.value=(value.valid()&1)!=0?value.tps()/100.0:null;tpsGauge.draw();
        iatGauge.value=(value.valid()&2)!=0?value.iat()/100.0:null;iatGauge.draw();
        smallValues.get("BATERIA").setText((value.valid()&4)!=0?String.format(Locale.ROOT,"%.2f V",value.voltage()/100.0):"— V");
        indicator("launch",(value.flags()&128)!=0?(value.state()&4)!=0:null);
    }
    void showCam(io.tunuino.protocol.CamPhaseClient.Phase phase){
        camPhase.setText(phase==null?"Cames: leitura indisponível neste firmware":phase.description());
    }
    private void indicator(String role,Boolean active){Label value=outputValues.get(role);value.setText(active==null?"—":active?"● ON":"● OFF");value.setStyle("-fx-font-size:10px;-fx-text-fill:"+(active!=null&&active?"#28de45":"#8b9aa7")+";");}
    void showOutputs(io.tunuino.protocol.EngineOutputClient.State state,byte[] profile){
        String[] roles={"pump","fan","relay"};
        for(int i=0;i<3;i++){int ch=profile!=null&&profile.length==128&&(profile[0]==1||profile[0]==2)?profile[22+i]&255:0;indicator(roles[i],ch>0&&ch<=16?(state.active()&(1L<<(ch-1)))!=0:null);}
        showOutputs(state);
    }
    void showOutputs(io.tunuino.protocol.EngineOutputClient.State state){
        outputs.setText(state.description()+"\n"+state.faultsDescription()
            +"\nSaídas ativas: "+state.activeChannels()
            +"\nInjeção: "+String.format(Locale.forLanguageTag("pt-PT"),"%.3f",state.injectionUs()/1000.0)+" ms");
    }
    void setEditAction(Consumer<MapSelection> action) {
        onEdit=java.util.Objects.requireNonNull(action);
    }
    private void editSelected(int row,int column) {
        CalibrationMap map=maps.get(selected);
        if(readingAllowed&&map!=null)onEdit.accept(new MapSelection(map,row,column));
    }
    void setConnection(DeviceIdentity identity,boolean connected,boolean available) {
        boolean active=connected&&identity!=null;
        if(mapOwner!=null&&(!active||!mapOwner.equals(identity)))clearMaps();
        mapOwner=active?identity:null;
        if(active!=online) {
            if(!active)clearMeasurements();
        }
        online=active;readingAllowed=active&&available;
        connection.setText(active?"●  ECU IDENTIFICADA":connected?"●  USB ABERTO":"●  USB DESLIGADO");
        connection.setStyle("-fx-text-fill:"+(active?"#20da7c":"#93aac2")+";");
        firmwareLabel.setText(identity==null?"Firmware —":"Firmware "+identity.firmwareVersion());
        readVe.setDisable(!readingAllowed);
        readSpark.setDisable(!readingAllowed);
        editMap.setDisable(!readingAllowed||!maps.containsKey(selected));
        if(!active) {
            outputs.setText("Estado das saídas indisponível");
            for(String role:outputValues.keySet())indicator(role,null);
            status.setText("Sem ligação a uma ECU identificada");
            source.setText("Nenhum valor de sensor disponível");
        } else {
            status.setText("Tunuino · Proteus v0.7 STM32F427");
            if(!readingAllowed)mapSource.setText("Leitura de mapas indisponível durante outra operação USB.");
        }
    }
    void setPort(String port) {
        selectedPort=port==null?"USB":port;
        portLabel.setText("PORTA "+selectedPort);
    }
    void monitoringStarted() {
        clearMeasurements();
        source.setText("Monitorização USB ativa · aguarda sensores válidos");
    }
    void showSample(Optional<LogSample> sample) {
        if(sample.isEmpty()) {
            clearMeasurements();
            source.setText("Amostra inválida ou indisponível.");
            return;
        }
        LogSample data=sample.get();
        latestSample=data;
        for(Channel channel:Channel.values())gauges.get(channel).setSample(data);
        lambdaValue.setText((data.validMask()&4)!=0?String.format(Locale.ROOT,"%.2f λ",data.lambda()):"— λ");
        if(liveHighlight.isSelected())drawHeatmap();
        source.setText("Telemetria USB real · relógio da ECU "+String.format(Locale.ROOT,"%.3f s",data.timeMs()/1000.0));
    }
    void monitoringStopped(String reason) {
        clearMeasurements();source.setText(reason);
    }
    void selectKind(CalibrationMap.Kind kind) {
        selected=java.util.Objects.requireNonNull(kind);
        veTab.setSelected(selected==CalibrationMap.Kind.VE);
        sparkTab.setSelected(selected==CalibrationMap.Kind.IGNITION);
        redrawMaps();
    }
    void showMap(CalibrationMap map) {
        if(map.columns()!=16||map.rows()!=16)
            throw new IllegalArgumentException("O Dashboard exige mapas nativos 16×16");
        maps.put(map.kind(),map);
        selected=map.kind();
        veTab.setSelected(selected==CalibrationMap.Kind.VE);
        sparkTab.setSelected(selected==CalibrationMap.Kind.IGNITION);
        mapSource.setText("Mapa "+selected+" lido da ECU · 16×16 · 256 células reais");
        redrawMaps();
    }
    void clearMaps() {
        maps.clear();mapSource.setText("Nenhum mapa recebido da ECU.");
        redrawMaps();
    }
    private void clearMeasurements() {
        camPhase.setText("Cames: dados atuais indisponíveis");
        latestSample=null;lambdaValue.setText("— λ");tpsGauge.clear();iatGauge.clear();smallValues.get("BATERIA").setText("— V");
        for(Gauge gauge:gauges.values())gauge.clear();
        drawHeatmap();
    }
    private void redrawMaps() {
        CalibrationMap map=maps.get(selected);
        editMap.setDisable(!readingAllowed||map==null);
        cellDetails.setText(map==null?"Lê uma tabela da ECU para consultar e editar células.":
            "Duplo clique numa célula para editar; a gravação na ECU é explícita no editor.");
        viewCaption.setText(map==null?"TABELA "+selected+" 16×16 · SEM DADOS":"TABELA "+selected+" 16×16 · DADOS DA ECU");
        drawHeatmap();drawIgnition();drawSurface();
    }
    private static double clamp(double v) {return Math.max(0,Math.min(1,v));}
    private static Color heat(double ratio) {
        double x=clamp(ratio);
        if(x<.28)return Color.web("#166bb7").interpolate(Color.web("#38c0e1"),x/.28);
        if(x<.52)return Color.web("#38c0e1").interpolate(Color.web("#47c783"),(x-.28)/.24);
        if(x<.75)return Color.web("#47c783").interpolate(Color.web("#f1d153"),(x-.52)/.23);
        return Color.web("#f1d153").interpolate(Color.web("#f05b4b"),(x-.75)/.25);
    }
    private void drawHeatmap() {
        GraphicsContext g=tableCanvas.getGraphicsContext2D();
        double w=tableCanvas.getWidth(),h=tableCanvas.getHeight();
        if(w<=0||h<=0)return;
        g.setFill(Color.web("#090e13"));g.fillRect(0,0,w,h);
        CalibrationMap map=maps.get(selected);
        double left=72,top=38,cw=(w-left-8)/16,ch=(h-top-8)/16;
        g.setTextAlign(TextAlignment.CENTER);g.setFont(Font.font("Segoe UI",11));
        g.setFill(DIM);
        g.fillText("MAP / RPM",left/2,17);
        for(int col=0;col<16;++col)g.fillText(map==null?"—":Integer.toString(map.rpmAt(col)),left+cw*(col+.5),27);
        double min=Double.POSITIVE_INFINITY,max=Double.NEGATIVE_INFINITY;
        if(map!=null)for(int r=0;r<16;++r)for(int c=0;c<16;++c){
            min=Math.min(min,map.cell(r,c));max=Math.max(max,map.cell(r,c));
        }
        for(int r=0;r<16;++r){
            g.setFill(DIM);
            g.fillText(map==null?"—":String.format(Locale.ROOT,"%.0f",map.loadKpaAt(r)),left/2,top+ch*(r+.67));
            for(int col=0;col<16;++col){
                double x=left+col*cw,y=top+r*ch;
                double value=map==null?0:map.cell(r,col);
                g.setFill(map==null?Color.web("#18293a"):heat(max==min?.45:(value-min)/(max-min)));
                g.fillRect(x+1,y+1,Math.max(0,cw-2),Math.max(0,ch-2));
                if(map!=null&&cw>24&&ch>=10){
                    g.setFill(Color.web("#071321"));
                    g.setFont(Font.font("Segoe UI",cw<46?9:10));
                    g.fillText(String.format(Locale.ROOT,"%.1f",value),x+cw/2,y+ch*.71);
                }
            }
        }
        // No extra serial read: the current 2×2 interpolation cell follows
        // real RPM/MAP samples while the map remains the last verified snapshot.
        if(map!=null && latestSample!=null && liveHighlight.isSelected()
                &&(latestSample.validMask()&3)==3
                &&latestSample.rpm()>=map.rpmAt(0) && latestSample.rpm()<=map.rpmAt(15)
                &&latestSample.mapKpa()>=map.loadKpaAt(0)
                &&latestSample.mapKpa()<=map.loadKpaAt(15)) {
            int col=0,row=0;
            while(col<14&&latestSample.rpm()>map.rpmAt(col+1))++col;
            while(row<14&&latestSample.mapKpa()>map.loadKpaAt(row+1))++row;
            g.setStroke(Color.web("#ffffff"));
            g.setLineWidth(2.3);
            g.strokeRect(left+col*cw+1,top+row*ch+1,2*cw-2,2*ch-2);
        }
    }
    /** Secondary ignition 16x16 panel; no values are fabricated before a map read. */
    private void drawIgnition() {
        GraphicsContext g=ignitionOverview.getGraphicsContext2D();
        double w=ignitionOverview.getWidth(),h=ignitionOverview.getHeight();
        if(w<=0||h<=0)return;
        g.setFill(Color.web("#090e13"));g.fillRect(0,0,w,h);
        CalibrationMap map=maps.get(CalibrationMap.Kind.IGNITION);
        double left=70,top=36,cw=(w-left-8)/16,ch=(h-top-8)/16;
        g.setTextAlign(TextAlignment.CENTER);
        g.setFont(Font.font("Segoe UI",9));g.setFill(DIM);
        g.fillText("MAP / RPM",left/2,17);
        for(int col=0;col<16;++col)
            g.fillText(map==null?"—":Integer.toString(map.rpmAt(col)),left+cw*(col+.5),26);
        double min=Double.POSITIVE_INFINITY,max=Double.NEGATIVE_INFINITY;
        if(map!=null)for(int y=0;y<16;++y)for(int x=0;x<16;++x) {
            min=Math.min(min,map.cell(y,x));max=Math.max(max,map.cell(y,x));
        }
        for(int row=0;row<16;++row) {
            g.setFill(DIM);g.setFont(Font.font("Segoe UI",9));
            g.fillText(map==null?"—":String.format(Locale.ROOT,"%.0f",map.loadKpaAt(row)),
                left/2,top+ch*(row+.69));
            for(int col=0;col<16;++col) {
                double x=left+col*cw,y=top+row*ch;
                double val=map==null?0:map.cell(row,col);
                g.setFill(map==null?Color.web("#1a2937"):heat(max==min?.45:(val-min)/(max-min)));
                g.fillRect(x+1,y+1,Math.max(0,cw-2),Math.max(0,ch-2));
                if(map!=null&&cw>=24&&ch>=10) {
                    g.setFill(Color.web("#071321"));g.setFont(Font.font("Segoe UI",9));
                    g.fillText(String.format(Locale.ROOT,"%.1f",val),x+cw/2,y+ch*.72);
                }
            }
        }
    }
    private void drawSurface() {
        GraphicsContext g=surfaceCanvas.getGraphicsContext2D();
        double w=surfaceCanvas.getWidth(),h=surfaceCanvas.getHeight();
        if(w<=0||h<=0)return;
        g.setFill(Color.web("#090e13"));g.fillRect(0,0,w,h);
        CalibrationMap map=maps.get(selected);
        if(map==null) {
            g.setFill(DIM);g.setTextAlign(TextAlignment.CENTER);g.setFont(Font.font("Segoe UI",14));
            g.fillText("Lê um mapa 16×16 da ECU para visualizar a superfície.",w/2,h/2);
            return;
        }
        double min=Double.POSITIVE_INFINITY,max=Double.NEGATIVE_INFINITY;
        for(int y=0;y<16;++y)for(int x=0;x<16;++x){
            min=Math.min(min,map.cell(y,x));max=Math.max(max,map.cell(y,x));
        }
        double sx=Math.min(w/40,14),sy=Math.min(h/48,8);
        double ox=w*.5,oy=h*.6;
        double[][] px=new double[16][16],py=new double[16][16];
        for(int y=0;y<16;++y)for(int x=0;x<16;++x){
            double elevation=(max==min?.3:(map.cell(y,x)-min)/(max-min))*h*.38;
            px[y][x]=ox+(x-y)*sx;py[y][x]=oy+(x+y)*sy-elevation;
        }
        for(int y=0;y<15;++y)for(int x=0;x<15;++x) {
            double[] xs={px[y][x],px[y][x+1],px[y+1][x+1],px[y+1][x]};
            double[] ys={py[y][x],py[y][x+1],py[y+1][x+1],py[y+1][x]};
            double value=(map.cell(y,x)+map.cell(y,x+1)+map.cell(y+1,x)+map.cell(y+1,x+1))*.25;
            g.setFill(heat(max==min?.4:(value-min)/(max-min)));g.fillPolygon(xs,ys,4);
            g.setStroke(Color.web("#152e40",.63));g.setLineWidth(.6);g.strokePolygon(xs,ys,4);
        }
        g.setFill(DIM);g.setFont(Font.font("Segoe UI",11));g.setTextAlign(TextAlignment.CENTER);
        g.fillText("RPM  →",w*.72,h*.91);g.fillText("←  MAP (kPa)",w*.24,h*.91);
        g.fillText(selected==CalibrationMap.Kind.VE?"VE (%)":"Ignição (°)",w*.5,18);
    }
    private static final class Gauge extends VBox {
        private final Canvas canvas=new Canvas(160,150);
        private final String title,unit;
        private final double minimum,maximum;
        private final Color accent;
        private Double value;
        Gauge(Channel channel) {
            this(switch(channel){case RPM->"RPM";case MAP->"MAP";case LAMBDA->"LAMBDA 1";case COOLANT->"TEMP. MOTOR";},
                switch(channel){case RPM->"rpm";case MAP->"kPa";case LAMBDA->"λ";case COOLANT->"°C";},
                switch(channel){case RPM->"#249efa";case MAP->"#4dbeef";case LAMBDA->"#e95058";case COOLANT->"#f2be41";},
                switch(channel){case RPM->0;case MAP->0;case LAMBDA->0.6;case COOLANT->-20;},
                switch(channel){case RPM->8000;case MAP->250;case LAMBDA->1.6;case COOLANT->150;});
        }
        Gauge(String title,String unit,String color,double min,double max) {
            this.title=title;this.unit=unit;this.minimum=min;this.maximum=max;this.accent=Color.web(color);
            getStyleClass().add("dashboard-gauge");
            setAlignment(Pos.CENTER);
            setPrefSize(180,158);setMinWidth(96);
            canvas.widthProperty().bind(widthProperty().subtract(20));
            canvas.setHeight(150);
            canvas.widthProperty().addListener((property,before,width)->draw());
            getChildren().add(canvas);
            draw();
        }
        void setSample(LogSample sample) {
            boolean valid;double number;
            switch(title) {
                case "RPM" -> {valid=(sample.validMask()&1)!=0;number=sample.rpm();}
                case "MAP" -> {valid=(sample.validMask()&2)!=0;number=sample.mapKpa();}
                case "LAMBDA 1" -> {valid=(sample.validMask()&4)!=0;number=sample.lambda();}
                case "TEMP. MOTOR" -> {valid=(sample.validMask()&8)!=0;number=sample.coolantC();}
                default -> {valid=false;number=0;}
            }
            value=valid?number:null;draw();
        }
        void clear(){value=null;draw();}
        private void draw() {
            GraphicsContext g=canvas.getGraphicsContext2D();
            double w=canvas.getWidth(),h=canvas.getHeight(),cx=w*.5,cy=h*.62,r=Math.min(76,w*.46);
            if(w<=0)return;
            g.clearRect(0,0,w,h);
            g.setLineWidth(9);g.setStroke(Color.web("#2c4158"));
            g.strokeArc(cx-r,cy-r,2*r,2*r,205,-230,ArcType.OPEN);
            g.setStroke(accent);g.strokeArc(cx-r,cy-r,2*r,2*r,205,-230,ArcType.OPEN);
            if(title.equals("RPM")){g.setStroke(Color.web("#e6323b"));g.strokeArc(cx-r,cy-r,2*r,2*r,30,-55,ArcType.OPEN);}
            g.setStroke(Color.web("#aac0d8"));g.setLineWidth(1);
            for(int tick=0;tick<=10;++tick) {
                double angle=Math.toRadians(205-tick*23);
                double x1=cx+Math.cos(angle)*(r-12),y1=cy-Math.sin(angle)*(r-12);
                double x2=cx+Math.cos(angle)*(r-4),y2=cy-Math.sin(angle)*(r-4);
                g.strokeLine(x1,y1,x2,y2);
                if(tick%2==0){g.setFill(WHITE);g.setFont(Font.font("Segoe UI",9));g.setTextAlign(TextAlignment.CENTER);
                    double n=minimum+(maximum-minimum)*tick/10.0;
                    g.fillText(String.format(Locale.ROOT,title.equals("LAMBDA 1")?"%.1f":"%.0f",title.equals("RPM")?n/1000:n),cx+Math.cos(angle)*(r-24),cy-Math.sin(angle)*(r-24)+3);}

            }
            if(value!=null) {
                double angle=Math.toRadians(205-230*clamp((value-minimum)/(maximum-minimum)));
                g.setStroke(accent);g.setLineWidth(2.5);
                g.strokeLine(cx,cy-12,cx+Math.cos(angle)*(r-15),cy-Math.sin(angle)*(r-15));
                g.setFill(accent);g.fillOval(cx-3,cy-15,6,6);
            }
            g.setTextAlign(TextAlignment.CENTER);
            g.setFill(Color.web("#c8d9ec"));g.setFont(Font.font("Segoe UI",11));
            g.fillText(title,cx,h-4);
            g.setFill(WHITE);g.setFont(Font.font("Segoe UI",28));
            String formatted=value==null?"—":unit.equals("λ")?
                String.format(Locale.ROOT,"%.2f",value):String.format(Locale.ROOT,"%.0f",value);
            g.fillText(formatted,cx,cy+20);
            g.setFill(DIM);g.setFont(Font.font("Segoe UI",11));
            g.fillText(unit,cx,cy+36);

        }
    }
}
