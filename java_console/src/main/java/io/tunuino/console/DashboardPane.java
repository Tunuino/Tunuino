/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.calibration.CalibrationMap;
import io.tunuino.datalog.LogSample;
import io.tunuino.protocol.DeviceIdentity;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayDeque;
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
import javafx.scene.layout.StackPane;
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
    private static final DateTimeFormatter TIME=DateTimeFormatter.ofPattern("HH:mm:ss");
    private final EnumMap<Channel,Gauge> gauges=new EnumMap<>(Channel.class);
    private final Label status=new Label("ECU desligada · aguarda ligação USB");
    private final Label source=new Label("Sem valores de sensores disponíveis");
    private final Label mapSource=new Label("Liga e identifica a ECU para ler as tabelas.");
    private final Label viewCaption=new Label("Mapas da ECU · a aguardar leitura real");
    private final Label connection=new Label("●  USB DESLIGADO");
    private final Label records=new Label("REGISTO");
    private final VBox eventRows=new VBox(7);
    private final ArrayDeque<String> history=new ArrayDeque<>();
    private final Canvas tableCanvas=new Canvas(840,315);
    private final Canvas surfaceCanvas=new Canvas(470,285);
    private final Canvas ignitionOverview=new Canvas(740,285);
    private final StackPane ignitionHost=new StackPane(ignitionOverview);
    private final StackPane surfaceHost=new StackPane(surfaceCanvas);
    private final StackPane tableHost=new StackPane(tableCanvas);
    private final ToggleButton veTab=new ToggleButton("VE 16×16");
    private final ToggleButton sparkTab=new ToggleButton("Ignição 16×16");
    private final Button readVe=new Button("Ler VE da ECU");
    private final Button readSpark=new Button("Ler ignição da ECU");
    private final CheckBox show3d=new CheckBox("Vista 3D");
    private final CheckBox liveHighlight=new CheckBox("Realçar célula ativa");
    private final Label cellDetails=new Label("Seleciona uma célula do mapa para consultar o valor.");
    private final EnumMap<CalibrationMap.Kind,CalibrationMap> maps=new EnumMap<>(CalibrationMap.Kind.class);
    private final FlowPane gaugeRow=new FlowPane(10,8);
    private final FlowPane smallRow=new FlowPane(9,9);
    private Consumer<CalibrationMap.Kind> onRead=kind->{};
    private CalibrationMap.Kind selected=CalibrationMap.Kind.VE;
    private boolean online;
    private LogSample latestSample;
    private boolean readingAllowed;
    private String selectedPort="USB";
    private final Label portLabel=new Label("PORTA —");
    private final Label firmwareLabel=new Label("Firmware —");

    DashboardPane() { this(false); }

    /** Maps-only cockpit reuses precisely the same real 16x16 rendering logic. */
    DashboardPane(boolean mapsOnly) {
        setId(mapsOnly?"dashboard-map-workspace":"dashboard");
        getStyleClass().add("ecu-dashboard");
        setSpacing(12);
        setPadding(new Insets(8,14,18,14));
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

        for(Channel channel:Channel.values()) {
            Gauge gauge=new Gauge(channel);
            gauges.put(channel,gauge);
            gaugeRow.getChildren().add(gauge);
        }
        // Other gauges are explicit placeholders: firmware does not measure them.
        gaugeRow.getChildren().add(new Gauge("TPS","%","#f7ba37",0,100));
        gaugeRow.getChildren().add(new Gauge("IAT","°C","#59b9ee",-20,120));
        gaugeRow.setPrefWrapLength(1240);
        smallRow.setPrefWrapLength(1240);
        for(String[] entry:new String[][]{
            {"BATERIA","— V"},{"PRESSÃO DE COMBUSTÍVEL","— bar"},
            {"PRESSÃO DE ÓLEO","— bar"},{"TEMP. ÓLEO","— °C"},
            {"TEMP. COMBUSTÍVEL","— °C"},{"LAMBDA 2","— λ"},
            {"VELOCIDADE","— km/h"}}) {
            Label value=new Label(entry[1]);value.getStyleClass().add("dashboard-small-value");
            Label name=new Label(entry[0]);name.getStyleClass().add("dashboard-small-title");
            VBox tile=new VBox(5,name,value,new Label("Sem canal na ECU"));
            tile.getChildren().get(2).getStyleClass().add("dashboard-small-note");
            tile.getStyleClass().add("dashboard-small-card");
            tile.setPrefWidth(168);smallRow.getChildren().add(tile);
        }

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
        readVe.setOnAction(event->onRead.accept(CalibrationMap.Kind.VE));
        readSpark.setOnAction(event->onRead.accept(CalibrationMap.Kind.IGNITION));
        HBox controls=new HBox(8,veTab,sparkTab,new javafx.scene.layout.Region(),
            readVe,readSpark,liveHighlight,show3d);
        HBox.setHgrow(controls.getChildren().get(2),Priority.ALWAYS);
        controls.setAlignment(Pos.CENTER_LEFT);
        controls.getStyleClass().add("dashboard-table-actions");
        mapSource.getStyleClass().add("dashboard-subtext");mapSource.setWrapText(true);
        tableHost.getStyleClass().add("dashboard-table-host");
        tableHost.setMinHeight(310);tableHost.setPrefHeight(325);
        cellDetails.setWrapText(true);
        cellDetails.getStyleClass().add("dashboard-subtext");
        tableCanvas.setOnMouseMoved(event->{
            CalibrationMap map=maps.get(selected);
            if(map==null)return;
            double w=tableCanvas.getWidth(),h=tableCanvas.getHeight();
            if(w<=80||h<=46)return;
            double cw=(w-80)/16,ch=(h-46)/16;
            int col=(int)((event.getX()-72)/cw),row=(int)((event.getY()-38)/ch);
            if(col>=0&&col<16&&row>=0&&row<16)
                cellDetails.setText(String.format(Locale.ROOT,
                    "RPM %d · MAP %.1f kPa · %s %.3f %s",
                    map.rpmAt(col),map.loadKpaAt(row),selected,map.cell(row,col),
                    selected==CalibrationMap.Kind.VE?"%":"°"));
        });
        tableCanvas.widthProperty().bind(tableHost.widthProperty());
        tableCanvas.heightProperty().bind(tableHost.heightProperty());
        tableHost.widthProperty().addListener((a,b,c)->drawHeatmap());
        tableHost.heightProperty().addListener((a,b,c)->drawHeatmap());
        VBox tablePanel=new VBox(7,controls,viewCaption,tableHost,cellDetails,mapSource);
        tablePanel.getStyleClass().add("dashboard-card");
        tablePanel.setMinWidth(560);VBox.setVgrow(tableHost,Priority.NEVER);
        tablePanel.setPrefWidth(840);
        HBox.setHgrow(tablePanel,Priority.ALWAYS);

        records.getStyleClass().add("dashboard-small-title");
        Label note=new Label("Saídas do motor: não implementadas neste firmware.");
        note.setWrapText(true);note.getStyleClass().add("dashboard-subtext");
        Label outputs=new Label("Bomba combustível     —\nRelé principal               —\nVentoinha                      —\nIgnição / injetores      DESATIVADOS");
        outputs.setId("dashboard-engine-status");
        outputs.getStyleClass().add("dashboard-output-status");
        eventRows.setId("dashboard-event-list");
        VBox side=new VBox(13,new Label("ESTADO DO MOTOR"),note,outputs,records,eventRows);
        side.getChildren().get(0).getStyleClass().add("dashboard-small-title");
        side.getStyleClass().add("dashboard-card");
        side.setMinWidth(235);side.setPrefWidth(250);
        // Wide monitors show the map and engine diagnostics side by side.
        // Smaller windows wrap the status card rather than clipping the grid.
        FlowPane center=new FlowPane(10,10,tablePanel,side);
        tablePanel.prefWidthProperty().bind(widthProperty().subtract(290));

        Label surfaceTitle=new Label("MAPA 3D  ·  VISUALIZAÇÃO DAS CÉLULAS");
        surfaceTitle.getStyleClass().add("dashboard-small-title");
        surfaceHost.getStyleClass().add("dashboard-table-host");
        surfaceHost.setMinHeight(260);surfaceHost.setPrefHeight(285);
        surfaceCanvas.widthProperty().bind(surfaceHost.widthProperty());
        surfaceCanvas.heightProperty().bind(surfaceHost.heightProperty());
        surfaceHost.widthProperty().addListener((a,b,c)->drawSurface());
        surfaceHost.heightProperty().addListener((a,b,c)->drawSurface());
        VBox surfacePanel=new VBox(8,surfaceTitle,surfaceHost);
        surfacePanel.getStyleClass().add("dashboard-card");
        VBox.setVgrow(surfaceHost,Priority.NEVER);
        VBox.setVgrow(surfacePanel,Priority.NEVER);
        surfacePanel.setMinWidth(350);
        ignitionHost.getStyleClass().add("dashboard-table-host");
        ignitionHost.setMinHeight(260);ignitionHost.setPrefHeight(285);
        ignitionOverview.widthProperty().bind(ignitionHost.widthProperty());
        ignitionOverview.heightProperty().bind(ignitionHost.heightProperty());
        ignitionHost.widthProperty().addListener((a,b,c)->drawIgnition());
        ignitionHost.heightProperty().addListener((a,b,c)->drawIgnition());
        Label secondTitle=new Label("TABELA DE IGNIÇÃO · 16×16");
        secondTitle.getStyleClass().add("dashboard-small-title");
        VBox ignitionPanel=new VBox(8,secondTitle,ignitionHost);
        ignitionPanel.getStyleClass().add("dashboard-card");
        ignitionPanel.setMinWidth(560);
        ignitionPanel.prefWidthProperty().bind(widthProperty().subtract(465));
        surfacePanel.setPrefWidth(435);
        FlowPane lower=new FlowPane(10,10,ignitionPanel,surfacePanel);
        if(mapsOnly) {
            Label heading=new Label("TABELAS NATIVAS 16×16 · ECU");
            heading.getStyleClass().add("dashboard-kicker");
            getChildren().addAll(heading,center,surfacePanel);
        } else {
            getChildren().addAll(header,gaugeRow,smallRow,center,lower);
        }
        addEvent("Painel inicializado. Sem valores simulados.");
        setConnection(null,false,false);
        redrawMaps();
    }

    void setReadAction(Consumer<CalibrationMap.Kind> action) {
        onRead=java.util.Objects.requireNonNull(action);
    }
    void setConnection(DeviceIdentity identity,boolean connected,boolean available) {
        boolean active=connected&&identity!=null;
        if(active!=online) {
            addEvent(active?"ECU identificada.":"Ligação ou identificação da ECU indisponível.");
            if(!active)clearMeasurements();
        }
        online=active;readingAllowed=active&&available;
        connection.setText(active?"●  ECU IDENTIFICADA":connected?"●  USB ABERTO":"●  USB DESLIGADO");
        connection.setStyle("-fx-text-fill:"+(active?"#20da7c":"#93aac2")+";");
        firmwareLabel.setText(identity==null?"Firmware —":"Firmware "+identity.firmwareVersion());
        readVe.setDisable(!readingAllowed);
        readSpark.setDisable(!readingAllowed);
        if(!active) {
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
        addEvent("Monitorização iniciada.");
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
        if(liveHighlight.isSelected())drawHeatmap();
        source.setText("Telemetria USB real · relógio da ECU "+String.format(Locale.ROOT,"%.3f s",data.timeMs()/1000.0));
    }
    void monitoringStopped(String reason) {
        clearMeasurements();source.setText(reason);addEvent(reason);
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
        addEvent("Mapa "+selected+" 16×16 recebido e verificado.");
        redrawMaps();
    }
    void clearMaps() {
        maps.clear();mapSource.setText("Nenhum mapa recebido da ECU.");
        redrawMaps();
    }
    void event(String message) { addEvent(message); }
    private void clearMeasurements() {
        latestSample=null;
        for(Gauge gauge:gauges.values())gauge.clear();
        drawHeatmap();
    }
    private void redrawMaps() {
        CalibrationMap map=maps.get(selected);
        viewCaption.setText(map==null?"TABELA "+selected+" 16×16 · SEM DADOS":"TABELA "+selected+" 16×16 · DADOS DA ECU");
        drawHeatmap();drawIgnition();drawSurface();
    }
    private void addEvent(String message) {
        history.addFirst("["+LocalTime.now().format(TIME)+"] "+message);
        while(history.size()>8)history.removeLast();
        eventRows.getChildren().clear();
        for(String entry:history) {
            Label line=new Label(entry);line.setWrapText(true);
            line.getStyleClass().add("dashboard-event");eventRows.getChildren().add(line);
        }
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
        g.setFill(Color.web("#0d1928"));g.fillRect(0,0,w,h);
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
                if(map!=null&&cw>32&&ch>=14){
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
        g.setFill(Color.web("#0d1928"));g.fillRect(0,0,w,h);
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
                if(map!=null&&cw>=34&&ch>=13) {
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
        g.setFill(Color.web("#0d1928"));g.fillRect(0,0,w,h);
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
            setPrefSize(180,184);setMinWidth(150);
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
            double w=canvas.getWidth(),h=canvas.getHeight(),cx=w*.5,cy=h*.59,r=56;
            g.clearRect(0,0,w,h);
            g.setLineWidth(9);g.setStroke(Color.web("#2c4158"));
            g.strokeArc(cx-r,cy-r,2*r,2*r,205,-230,ArcType.OPEN);
            g.setStroke(accent);
            if(value!=null)g.strokeArc(cx-r,cy-r,2*r,2*r,205,-230*clamp((value-minimum)/(maximum-minimum)),ArcType.OPEN);
            g.setStroke(Color.web("#aac0d8"));g.setLineWidth(1);
            for(int tick=0;tick<=10;++tick) {
                double angle=Math.toRadians(205-tick*23);
                double x1=cx+Math.cos(angle)*(r-12),y1=cy-Math.sin(angle)*(r-12);
                double x2=cx+Math.cos(angle)*(r-4),y2=cy-Math.sin(angle)*(r-4);
                g.strokeLine(x1,y1,x2,y2);
            }
            if(value!=null) {
                double angle=Math.toRadians(205-230*clamp((value-minimum)/(maximum-minimum)));
                g.setStroke(accent);g.setLineWidth(2.5);
                g.strokeLine(cx,cy,cx+Math.cos(angle)*(r-15),cy-Math.sin(angle)*(r-15));
                g.setFill(accent);g.fillOval(cx-3,cy-3,6,6);
            }
            g.setTextAlign(TextAlignment.CENTER);
            g.setFill(Color.web("#c8d9ec"));g.setFont(Font.font("Segoe UI",11));
            g.fillText(title,cx,18);
            g.setFill(WHITE);g.setFont(Font.font("Segoe UI",28));
            String formatted=value==null?"—":unit.equals("λ")?
                String.format(Locale.ROOT,"%.2f",value):String.format(Locale.ROOT,"%.0f",value);
            g.fillText(formatted,cx,cy+8);
            g.setFill(DIM);g.setFont(Font.font("Segoe UI",11));
            g.fillText(unit,cx,cy+29);
            if(value==null){g.setFill(DIM);g.setFont(Font.font("Segoe UI",9));g.fillText("Sem sinal",cx,h-5);}
        }
    }
}
