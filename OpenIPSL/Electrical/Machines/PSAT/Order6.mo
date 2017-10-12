within OpenIPSL.Electrical.Machines.PSAT;
model Order6
  extends BaseClasses.baseMachine(
    vf(start=vf00),
    vf0(start=vf00),
    xq0=xq);
  parameter Real xd=1.9 "d-axis synchronous reactance (pu)"
    annotation (Dialog(group="Machine parameters"));
  parameter Real xq=1.7 "q-axis synchronous reactance (pu)"
    annotation (Dialog(group="Machine parameters"));
  parameter Real x1q=0.5 "q-axis transient reactance (pu)"
    annotation (Dialog(group="Machine parameters"));
  parameter Real x2d=0.204 "d-axis sub-transient reactance (pu)"
    annotation (Dialog(group="Machine parameters"));
  parameter Real x2q=0.3 "q-axis sub-transient reactance (pu)"
    annotation (Dialog(group="Machine parameters"));
  parameter Real T1d0=8 "d-axis open circuit transient time constant (s)"
    annotation (Dialog(group="Machine parameters"));
  parameter Real T1q0=0.8 "q-axis open circuit transient time constant (s)"
    annotation (Dialog(group="Machine parameters"));
  parameter Real T2d0=0.04 "d-axis open circuit transient time constant (s)"
    annotation (Dialog(group="Machine parameters"));
  parameter Real T2q0=0.02 "q-axis open circuit transient time constant (s)"
    annotation (Dialog(group="Machine parameters"));
  parameter Real Taa=2e-3 "d-axis aditional leakage time constant (s)"
    annotation (Dialog(group="Machine parameters"));

  Real e1q(start=e1q0) "q-axis transient voltage";
  Real e1d(start=e1d0) "d-axis transient voltage";
  Real e2q(start=e2q0) "q-axis sub-transient voltage";
  Real e2d(start=e2d0) "d-axis sub-transient voltage";

  parameter Real e2q0=vq0 + ra*iq0 + x2d*id0 "Initialitation";
  parameter Real e2d0=vd0 + ra*id0 - x2q*iq0 "Initialitation";
  parameter Real e1d0=(xq - x1q - T2q0*x2q*(xq - x1q)/(T1q0*x1q))*iq0;
  parameter Real K1=xd - x1d - T2d0*x2d*(xd - x1d)/(T1d0*x1d);
  parameter Real K2=x1d - x2d + T2d0*x2d*(xd - x1d)/(T1d0*x1d);
  parameter Real e1q0=e2q0 + K2*id0 - Taa/T1d0*((K1 + K2)*id0 + e2q0);
  parameter Real vf00=(K1*id0 + e1q0)/(1 - Taa/T1d0);
initial equation
  der(e1d) = 0;
  // der(e1d) = 0;
  der(e2d) = 0;
  // der(e2q) = 0;
equation

  der(e1q) = ((-e1q) - (xd - x1d - T2d0/T1d0*x2d/x1d*(xd - x1d))*id + (1 - Taa/
    T1d0)*vf)/T1d0;
  der(e1d) = ((-e1d) + (xq - x1q - T2q0/T1q0*x2q/x1q*(xq - x1q))*iq)/T1q0;
  der(e2d) = ((-e2d) + e1d + (x1q - x2q + T2q0/T1q0*x2q/x1q*(xq - x1q))*iq)/
    T2q0;
  der(e2q) = ((-e2q) + e1q - (x1d - x2d + T2d0/T1d0*x2d/x1d*(xd - x1d))*id +
    Taa/T1d0*vf)/T2d0;
  e2q = vq + ra*iq + x2d*id;
  e2d = vd + ra*id - x2q*iq;
  vf0 = vf00;
  annotation (Icon(coordinateSystem(extent={{-100,-100},{100,100}},
          initialScale=0.1), graphics={Rectangle(
          visible=true,
          fillColor={255,255,255},
          extent={{-100.0,-100.0},{100.0,100.0}}),Text(
          origin={0,60},
          extent={{-60,-20},{60,20}},
          lineColor={28,108,200},
          textString="Order VI")}), Documentation(info="<html>
<table cellspacing=\"1\" cellpadding=\"1\" border=\"1\">
<tr>
<td><p>Reference</p></td>
<td>Generator Order VI, PSAT Manual</td>
</tr>
<tr>
<td><p>Last update</p></td>
<td>2015-10-02</td>
</tr>
<tr>
<td><p>Author</p></td>
<td><p>Le Qi, SmarTS Lab, KTH Royal Institute of Technology</p></td>
</tr>
<tr>
<td><p>Contact</p></td>
<td><p><a href=\"mailto:luigiv@kth.se\">luigiv@kth.se</a></p></td>
</tr>
</table>
</html>"));
end Order6;
