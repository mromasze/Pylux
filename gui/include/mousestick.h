// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_MOUSESTICK_H
#define CHIAKI_MOUSESTICK_H

#include <QList>
#include <QPoint>
#include <QPointF>

#include <stdint.h>

class QWindow;

/**
 * Turns mouse motion into right analog stick deflection ("mouse look").
 *
 * While the stream window is focused the cursor is hidden and warped back to the window
 * center before it can leave the window. Motion is averaged over a short window and the
 * stick is tilted proportionally to mouse speed, starting from a minimum deflection that
 * overcomes the game's own stick dead zone.
 */
class MouseStick
{
	public:
		MouseStick() = default;
		void SetParameters(float sensitivity, float deadzone_compensation);

		/**
		 * Feed a mouse move in global (screen) coordinates.
		 * @return true if the move produced stick motion
		 */
		bool Move(QWindow *window, const QPointF &global_pos, qint64 now_ns);

		/**
		 * Compute the current stick deflection.
		 * @return false once there is no recent motion left, i.e. the stick is at rest
		 */
		bool Update(qint64 now_ns, int16_t *x, int16_t *y);

	private:
		struct Sample
		{
			qint64 time_ns;
			QPointF delta;
		};

		float sensitivity = 1.0f;
		float deadzone_compensation = 0.15f;
		QList<Sample> samples;
		bool tracking = false;
		QPointF last_pos;
		qint64 last_move_ns = 0;
		bool warp_pending = false;
		QPoint warp_target;
};

#endif // CHIAKI_MOUSESTICK_H
