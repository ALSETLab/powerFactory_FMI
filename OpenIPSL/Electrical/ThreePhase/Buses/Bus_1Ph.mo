within OpenIPSL.Electrical.ThreePhase.Buses;
model Bus_1Ph "Single-phase bus"
  extends ThreePhaseComponent;
  OpenIPSL.Interfaces.PwPin p1(vr(start=V_1*cos(angle_1)),
                               vi(start=V_1*sin(angle_1)))
    annotation (Placement(transformation(extent={{-10,-10},{10,10}})));

  parameter Types.PerUnit V_1=1 "Voltage magnitude for phase 1"
    annotation (Dialog(group="Power flow data"));
  parameter Types.Angle angle_1=0 "Voltage angle for phase 1"
    annotation (Dialog(group="Power flow data"));
  Types.PerUnit V1(start=V_1) "Bus voltage magnitude for phase 1";
  Types.Angle angle1(start=angle_1) "Bus voltage angle for phase 1";

protected
  Real[1, 2] Vin=[p1.vr, p1.vi];

equation
  V1 = sqrt(Vin[1, 1]^2 + Vin[1, 2]^2);
  angle1 = atan2(Vin[1, 2], Vin[1, 1]);
  p1.ir = 0;
  p1.ii = 0;

  annotation (Icon(coordinateSystem(
        extent={{-100,-100},{100,100}},
        preserveAspectRatio=true,
        grid={2,2}), graphics={Rectangle(
          fillPattern=FillPattern.Solid,
          extent={{-10.0,-100.0},{10.0,100.0}}),Text(
          origin={0.9738,119.0625},
          fillPattern=FillPattern.Solid,
          extent={{-39.0262,-16.7966},{39.0262,16.7966}},
          textString="%name",
          fontName="Arial"),Text(
          origin={0.9738,-114.937},
          fillPattern=FillPattern.Solid,
          extent={{-39.0262,-16.7966},{39.0262,16.7966}},
          fontName="Arial",
          textString=DynamicSelect("0.0", String(V1, significantDigits=3)),
          lineColor={238,46,47}),Text(
          origin={0.9738,-140.937},
          fillPattern=FillPattern.Solid,
          extent={{-39.0262,-16.7966},{39.0262,16.7966}},
          fontName="Arial",
          textString=DynamicSelect("0.0", String(angle1, significantDigits=3)),
          lineColor={238,46,47})}),
          Documentation(info="<html>
<p>This is a single-phase bus model.</p>
<p>A bus represents a node in a power system. Therefore, this model can be used to verify voltage magnitude and angle in the single-phase nodes of the system.</p>
<p>Although it is not necessary, it is extremely recommended to connect one bus model between two other single-phase models.</p>
<p>Please, check if this bus model is the appropriate one for your system. For the connection of three- or two-phase models, three- or two-phase buses might be a better fit.</p>
</html>"));
end Bus_1Ph;
