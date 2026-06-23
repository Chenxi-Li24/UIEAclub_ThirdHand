/**
 * App - Main entry point
 */
(function () {
  'use strict';

  document.addEventListener('DOMContentLoaded', () => {
    console.log('[App] ThirdHand Web Control starting...');

    const ws = new WSClient();
    const viewport = document.getElementById('three-container');
    const scene = new SceneManager(viewport);
    const arm = new ArmModel(scene.scene);
    const panel = document.getElementById('control-panel');
    const ui = new UIControls(panel, ws, arm);
    ui.bindViewControls(scene);

    ws.connect();
    arm.setJointAngles([0, 0, 0, 0, 0, 0]);
    console.log('[App] Ready.');
  });
})();
