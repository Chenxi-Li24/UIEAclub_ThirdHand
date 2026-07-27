/**
 * ArmModel — Fairino FR5 参数化 3D 机械臂模型
 *
 * 基于 DH 参数构建 6 自由度运动学链
 * 使用 THREE.Group 层级嵌套实现关节旋转
 *
 * FR5 DH 参数 (标准 DH):
 *   Joint |    a(mm) | alpha(°) |   d(mm) | theta_offset(°)
 *   ------|----------|----------|---------|----------------
 *   J1    |      0   |   -90    |   397   |    0
 *   J2    |    444.5 |     0    |     0   |  -90
 *   J3    |     42   |    90    |     0   |    0
 *   J4    |      0   |   -90    |   451   |    0
 *   J5    |      0   |    90    |     0   |    0
 *   J6    |      0   |     0    |    84   |    0
 */
class ArmModel {
  constructor(scene) {
    this.scene = scene;
    this.jointAngles = [0, 0, 0, 0, 0, 0]; // 当前角度 (度)

    // DH 参数: [a(mm), alpha(rad), d(mm), thetaOffset(rad)]
    this.dh = [
      [0,      -Math.PI/2, 397,   0],
      [444.5,  0,          0,    -Math.PI/2],
      [42,     Math.PI/2,  0,     0],
      [0,      -Math.PI/2, 451,   0],
      [0,      Math.PI/2,  0,     0],
      [0,      0,          84,    0],
    ];

    // 连杆长度 (用于简化几何体)
    this.linkLengths = [397, 444.5, 42, 451, 0, 84];

    // 关节限位 [min, max] (度)
    this.jointLimits = [
      [-170, 170],
      [-135, 135],
      [-135, 135],
      [-170, 170],
      [-135, 135],
      [-360, 360],
    ];

    // 关节颜色
    this.colors = {
      base:   0x2d3a5c,
      j1:     0x64ffda,
      link1:  0x4a9eff,
      j2:     0xffd93d,
      link2:  0x4a9eff,
      j3:     0xff6b9d,
      link3:  0xff6b9d,
      j4:     0x6bcb77,
      link4:  0xc084fc,
      j5:     0xff9f43,
      j5link: 0xc084fc,
      j6:     0xffffff,
      ee:     0xff6b6b,
    };

    this.groups = [];
    this.eeIndicator = null;
    this._buildModel();
  }

  _buildModel() {
    const root = new THREE.Group();

    // 底座
    const base = this._createBase();
    root.add(base);

    // J1: 底座上方，绕 Y 旋转
    const j1Group = new THREE.Group();
    j1Group.position.set(0, 0, 0);
    j1Group.add(this._createJoint(18, this.colors.j1));

    const link1 = this._createLink(this.linkLengths[0], 16, this.colors.link1);
    link1.position.set(0, this.linkLengths[0] / 2, 0);
    j1Group.add(link1);

    // J2
    const j2Group = new THREE.Group();
    j2Group.position.set(0, this.linkLengths[0], 0);
    j2Group.add(this._createJoint(16, this.colors.j2));

    const link2 = this._createLink(this.linkLengths[1], 14, this.colors.link2);
    link2.position.set(0, 0, this.linkLengths[1] / 2);
    link2.rotation.x = Math.PI / 2;
    j2Group.add(link2);

    // J3
    const j3Group = new THREE.Group();
    j3Group.position.set(0, 0, this.linkLengths[1]);
    j3Group.add(this._createJoint(14, this.colors.j3));

    const link3 = this._createLink(this.linkLengths[2], 12, this.colors.link3);
    link3.position.set(this.linkLengths[2] / 2, 0, 0);
    link3.rotation.z = Math.PI / 2;
    j3Group.add(link3);

    // J4
    const j4Group = new THREE.Group();
    j4Group.position.set(this.linkLengths[2], 0, 0);
    j4Group.add(this._createJoint(12, this.colors.j4));

    const link4 = this._createLink(this.linkLengths[3], 10, this.colors.link4);
    link4.position.set(0, -this.linkLengths[3] / 2, 0);
    j4Group.add(link4);

    // J5
    const j5Group = new THREE.Group();
    j5Group.position.set(0, -this.linkLengths[3], 0);
    j5Group.add(this._createJoint(10, this.colors.j5));

    const link5 = this._createLink(40, 8, this.colors.j5link);
    link5.position.set(0, 0, 20);
    link5.rotation.x = Math.PI / 2;
    j5Group.add(link5);

    // J6
    const j6Group = new THREE.Group();
    j6Group.position.set(0, 0, 40);
    j6Group.add(this._createJoint(8, this.colors.j6));

    this.eeIndicator = this._createEE();
    this.eeIndicator.position.set(0, 0, this.linkLengths[5]);
    j6Group.add(this.eeIndicator);

    // 组装运动学链
    j5Group.add(j6Group);
    j4Group.add(j5Group);
    j3Group.add(j4Group);
    j2Group.add(j3Group);
    j1Group.add(j2Group);
    root.add(j1Group);

    this.groups = [j1Group, j2Group, j3Group, j4Group, j5Group, j6Group];
    this.scene.add(root);
    this.root = root;
  }

  _createBase() {
    const g = new THREE.Group();
    const mat = new THREE.MeshPhongMaterial({
      color: this.colors.base, shininess: 60, specular: 0x222222
    });

    const disk = new THREE.Mesh(new THREE.CylinderGeometry(80, 100, 30, 32), mat);
    disk.position.y = 15;
    disk.castShadow = true;
    disk.receiveShadow = true;
    g.add(disk);

    const col = new THREE.Mesh(new THREE.CylinderGeometry(40, 60, 50, 24), mat);
    col.position.y = 55;
    col.castShadow = true;
    g.add(col);

    return g;
  }

  _createJoint(radius, color) {
    const mat = new THREE.MeshPhongMaterial({
      color: color, shininess: 100, specular: 0x666666,
      emissive: color, emissiveIntensity: 0.15
    });
    const mesh = new THREE.Mesh(new THREE.SphereGeometry(radius, 24, 24), mat);
    mesh.castShadow = true;
    return mesh;
  }

  _createLink(length, radius, color) {
    const mat = new THREE.MeshPhongMaterial({
      color: color, shininess: 80, specular: 0x444444
    });
    const mesh = new THREE.Mesh(
      new THREE.CylinderGeometry(radius * 0.7, radius, length, 16), mat
    );
    mesh.castShadow = true;
    return mesh;
  }

  _createEE() {
    const g = new THREE.Group();

    const flangeMat = new THREE.MeshPhongMaterial({
      color: this.colors.ee, shininess: 80,
      emissive: this.colors.ee, emissiveIntensity: 0.2
    });
    const flange = new THREE.Mesh(
      new THREE.CylinderGeometry(25, 25, 8, 24), flangeMat
    );
    flange.rotation.x = Math.PI / 2;
    g.add(flange);

    const arrow = new THREE.Mesh(
      new THREE.ConeGeometry(8, 30, 12),
      new THREE.MeshPhongMaterial({ color: 0xff6b6b })
    );
    arrow.position.z = 20;
    arrow.rotation.x = -Math.PI / 2;
    g.add(arrow);

    return g;
  }

  /**
   * 设置关节角度 (度)
   * @param {number[]} angles - [J1, J2, J3, J4, J5, J6]
   */
  setJointAngles(angles) {
    if (!angles || angles.length < 6) return;

    for (let i = 0; i < 6; i++) {
      const [min, max] = this.jointLimits[i];
      angles[i] = Math.max(min, Math.min(max, angles[i]));
    }

    this.jointAngles = angles.slice();
    const rad = angles.map(a => a * Math.PI / 180);

    this.groups[0].rotation.set(0, rad[0], 0);
    this.groups[1].rotation.set(0, 0, rad[1] + Math.PI / 2);
    this.groups[2].rotation.set(0, 0, rad[2]);
    this.groups[3].rotation.set(rad[3], 0, 0);
    this.groups[4].rotation.set(0, 0, rad[4]);
    this.groups[5].rotation.set(rad[5], 0, 0);
  }

  getJointAngles() {
    return this.jointAngles.slice();
  }

  getEndEffectorPosition() {
    if (!this.eeIndicator) return { x: 0, y: 0, z: 0 };
    const pos = new THREE.Vector3();
    this.eeIndicator.getWorldPosition(pos);
    return { x: pos.x, y: pos.y, z: pos.z };
  }
}
