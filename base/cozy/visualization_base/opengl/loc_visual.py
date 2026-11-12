import sys
import time
import numpy as np

from OpenGL.GL import (GL_LINES, GL_DEPTH_TEST, GL_PROJECTION, GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_MODELVIEW,
                       glMatrixMode, glClear, glClearColor, glLoadIdentity, glViewport, glEnable, glPushMatrix,
                       glPopMatrix, glTranslatef, glMultMatrixf, glBegin, glEnd, glColor3f, glVertex3f, glLineWidth)
from OpenGL.GLUT import (GLUT_DOUBLE, GLUT_RGB, GLUT_DEPTH, glutInit, glutInitDisplayMode, glutInitWindowSize,
                         glutCreateWindow, glutDisplayFunc, glutReshapeFunc, glutMainLoop, glutSwapBuffers,
                         glutPostRedisplay)
from OpenGL.GLU import gluLookAt, gluPerspective

from ...loc_base.loc_infra import Pose, Point, Quaternion, LocHeader
from ...loc_base.loc_odom import Odom

_p0 = None
_p1 = None
_interpolated_poses = []
_current_step = 0


def _build_demo_poses(steps: int) -> tuple[Pose, Pose, list[Pose]]:
    p0 = Pose(header=LocHeader(stamp=0.0), position=Point(0, 0, 0), orientation=Quaternion(1, 0, 0, 0))
    p1 = Pose(
        header=LocHeader(stamp=1.0),
        position=Point(1, 1, 0),
        orientation=Quaternion(0.707, 0, 0.707, 0),
    )
    odom = Odom([p0, p1])
    interpolated_poses = [odom.interpolate(t / steps) for t in range(steps + 1)]
    return p0, p1, interpolated_poses


def draw_axes(pose: Pose, color: tuple[float, float, float]):
    glPushMatrix()
    glTranslatef(pose.position.x, pose.position.y, pose.position.z)
    q = pose.orientation
    qw, qx, qy, qz = q.qw, q.qx, q.qy, q.qz
    rot = np.array([
        [1 - 2 * qy**2 - 2 * qz**2, 2 * qx * qy - 2 * qz * qw, 2 * qx * qz + 2 * qy * qw, 0],
        [2 * qx * qy + 2 * qz * qw, 1 - 2 * qx**2 - 2 * qz**2, 2 * qy * qz - 2 * qx * qw, 0],
        [2 * qx * qz - 2 * qy * qw, 2 * qy * qz + 2 * qx * qw, 1 - 2 * qx**2 - 2 * qy**2, 0],
        [0, 0, 0, 1],
    ],
                   dtype=np.float32)
    glMultMatrixf(rot.T)
    glLineWidth(3)
    glBegin(GL_LINES)
    glColor3f(*color)
    glVertex3f(0, 0, 0)
    glVertex3f(1.0, 0, 0)
    glColor3f(*color)
    glVertex3f(0, 0, 0)
    glVertex3f(0, 1.0, 0)
    glColor3f(*color)
    glVertex3f(0, 0, 0)
    glVertex3f(0, 0, 1.0)
    glEnd()
    glPopMatrix()


def _display():
    global _current_step
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    glLoadIdentity()
    gluLookAt(5, 5, 5, 0, 0, 0, 0, 0, 1)
    draw_axes(_p0, (1, 0, 0))
    draw_axes(_p1, (0, 0, 1))
    draw_axes(_interpolated_poses[_current_step], (0, 1, 0))
    glutSwapBuffers()
    _current_step = (_current_step + 1) % len(_interpolated_poses)
    time.sleep(0.02)
    glutPostRedisplay()


def _reshape(w: int, h: int):
    glViewport(0, 0, w, h)
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    gluPerspective(45, float(w) / float(h), 0.1, 100)
    glMatrixMode(GL_MODELVIEW)


def run_odom_slerp_demo(steps: int = 100):
    global _p0, _p1, _interpolated_poses, _current_step
    _p0, _p1, _interpolated_poses = _build_demo_poses(steps)
    _current_step = 0

    glutInit(sys.argv)
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH)
    glutInitWindowSize(800, 600)
    glutCreateWindow(b'Odom Slerp Animation')
    glClearColor(0.9, 0.9, 0.9, 1.0)
    glEnable(GL_DEPTH_TEST)
    glutDisplayFunc(_display)
    glutReshapeFunc(_reshape)
    glutMainLoop()


def main():
    run_odom_slerp_demo()


__all__ = ['draw_axes', 'run_odom_slerp_demo', 'main']

if __name__ == '__main__':
    main()
